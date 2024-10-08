#include <chrono>
#include <string>

#include <Poco/NumberFormatter.h>

#include <Common/Stopwatch.h>
#include <libnuraft/async.hxx>

#include <Service/KeeperServer.h>
#include <Service/LoggerWrapper.h>
#include <Service/NuRaftStateMachine.h>
#include <Service/NuRaftStateManager.h>
#include <Service/ReadBufferFromNuRaftBuffer.h>
#include <ZooKeeper/ZooKeeperIO.h>

namespace RK
{
namespace ErrorCodes
{
    extern const int RAFT_ERROR;
}

using Poco::NumberFormatter;

KeeperServer::KeeperServer(
    const SettingsPtr & settings_,
    const Poco::Util::AbstractConfiguration & config_,
    KeeperResponsesQueue & responses_queue_,
    std::shared_ptr<RequestProcessor> request_processor_)
    : my_id(settings_->my_id)
    , settings(settings_)
    , config(config_)
    , log(&(Poco::Logger::get("KeeperServer")))
{
    state_manager = cs_new<NuRaftStateManager>(my_id, config, settings_);

    state_machine = nuraft::cs_new<NuRaftStateMachine>(
        responses_queue_,
        settings->raft_settings,
        settings->snapshot_dir,
        settings->log_dir,
        settings->snapshot_create_interval,
        settings->raft_settings->max_stored_snapshots,
        new_session_id_callback_mutex,
        new_session_id_callback,
        state_manager->load_log_store(),
        checkAndGetSuperDigest(settings->super_digest),
        MAX_OBJECT_NODE_SIZE,
        request_processor_);

#ifdef COMPATIBLE_MODE_ZOOKEEPER
    auto cluster_config = state_manager->getClusterConfig();

    String data;
    for (const auto & s : cluster_config->get_servers())
    {
        data += fmt::format("server.{}={}:participant\n", s->get_id(), s->get_endpoint());
    }
    data += "version=0";
    state_machine->getStore().getNode(ZOOKEEPER_CONFIG_NODE)->data = data;
#endif

}
namespace
{
    void initializeRaftParams(nuraft::raft_params & params, RaftSettingsPtr & raft_settings)
    {
        params.heart_beat_interval_ = raft_settings->heart_beat_interval_ms;
        params.election_timeout_lower_bound_ = raft_settings->election_timeout_lower_bound_ms;
        params.client_req_timeout_ = raft_settings->client_req_timeout_ms;
        params.election_timeout_upper_bound_ = raft_settings->election_timeout_upper_bound_ms;
        params.reserved_log_items_ = raft_settings->reserved_log_items;
        params.snapshot_distance_ = raft_settings->snapshot_distance;
        params.return_method_ = nuraft::raft_params::blocking;
        params.parallel_log_appending_ = raft_settings->log_fsync_mode == FsyncMode::FSYNC_PARALLEL;
        params.auto_forwarding_ = false;
    }
}

void KeeperServer::startup()
{
    auto raft_settings = settings->raft_settings;

    nuraft::raft_params params;
    initializeRaftParams(params, raft_settings);

    nuraft::asio_service::options asio_opts{};
    asio_opts.thread_pool_size_ = raft_settings->nuraft_thread_size;
    nuraft::raft_server::init_options init_options;

    init_options.skip_initial_election_timeout_ = state_manager->shouldStartAsFollower();
    init_options.raft_callback_ = [this](nuraft::cb_func::Type type, nuraft::cb_func::Param * param) { return callbackFunc(type, param); };

    UInt16 port = config.getInt("keeper.internal_port", 8103);

    raft_instance = launcher.init(
        state_machine,
        state_manager,
        nuraft::cs_new<LoggerWrapper>("NuRaft", raft_settings->raft_logs_level),
        port,
        asio_opts,
        params,
        init_options);

    if (!raft_instance)
        throw Exception(ErrorCodes::RAFT_ERROR, "Cannot initialized RAFT instance");

    /// used raft_instance notify_log_append_completion
    if (raft_settings->log_fsync_mode == FsyncMode::FSYNC_PARALLEL)
        dynamic_cast<NuRaftFileLogStore &>(*state_manager->load_log_store()).setRaftServer(raft_instance);
}

int32 KeeperServer::getLeader()
{
    return raft_instance->get_leader();
}

void KeeperServer::shutdown()
{
    LOG_INFO(log, "Shutting down NuRaft core.");
    if (!launcher.shutdown(settings->raft_settings->shutdown_timeout / 1000))
        LOG_WARNING(log, "Failed to shutdown NuRaft core in {}ms", settings->raft_settings->shutdown_timeout);

    LOG_INFO(log, "Flush Log store.");
    if (state_manager->load_log_store())
        state_manager->load_log_store()->flush();

    dynamic_cast<NuRaftFileLogStore &>(*state_manager->load_log_store()).shutdown();
    state_machine->shutdown();

    LOG_INFO(log, "Shut down NuRaft core done!");
}

ptr<nuraft::cmd_result<ptr<buffer>>> KeeperServer::pushRequestBatch(const std::vector<RequestForSession> & request_batch)
{
    LOG_DEBUG(log, "Push batch requests of size {}", request_batch.size());
    std::vector<ptr<buffer>> entries;
    for (const auto & request_session : request_batch)
    {
        LOG_TRACE(log, "Push request {}", request_session.toSimpleString());
        entries.push_back(serializeKeeperRequest(request_session));
    }
    /// append_entries write request
    ptr<nuraft::cmd_result<ptr<buffer>>> result = raft_instance->append_entries(entries);
    return result;
}

void KeeperServer::handleRemoteSession(int64_t session_id, int64_t expiration_time)
{
    state_machine->getStore().handleRemoteSession(session_id, expiration_time);
}

[[maybe_unused]] int64_t KeeperServer::getSessionTimeout(int64_t session_id)
{
    LOG_DEBUG(log, "New session timeout for {}", session_id);
    if (state_machine->getStore().containsSession(session_id))
    {
        return state_machine->getStore().getSessionAndTimeOut().find(session_id)->second;
    }
    else
    {
        LOG_WARNING(log, "Not found session timeout for {}", session_id);
        return -1;
    }
}

bool KeeperServer::isLeader() const
{
    return raft_instance->is_leader();
}


bool KeeperServer::isObserver() const
{
    auto cluster_config = state_manager->getClusterConfig();
    return cluster_config->get_server(my_id)->is_learner();
}

bool KeeperServer::isFollower() const
{
    return !isLeader() && !isObserver();
}

bool KeeperServer::isLeaderAlive() const
{
    /// nuraft leader_ and role_ not sync
    return raft_instance->is_leader_alive() && raft_instance->get_leader() != -1;
}

uint64_t KeeperServer::getFollowerCount() const
{
    return raft_instance->get_peer_info_all().size();
}

uint64_t KeeperServer::getSyncedFollowerCount() const
{
    uint64_t last_log_idx = raft_instance->get_last_log_idx();
    const auto followers = raft_instance->get_peer_info_all();

    uint64_t stale_followers = 0;

    const uint64_t stale_follower_gap = raft_instance->get_current_params().stale_log_gap_;
    for (const auto & fl : followers)
    {
        if (last_log_idx > fl.last_log_idx_ + stale_follower_gap)
            stale_followers++;
    }
    return followers.size() - stale_followers;
}

nuraft::cb_func::ReturnCode KeeperServer::callbackFunc(nuraft::cb_func::Type type, nuraft::cb_func::Param * /* param */)
{
    if (type == nuraft::cb_func::Type::BecomeFresh || type == nuraft::cb_func::Type::BecomeLeader)
    {
        std::unique_lock lock(initialized_mutex);
        initialized_flag = true;
        initialized_cv.notify_all();
    }
    else if (type == nuraft::cb_func::NewConfig)
    {
        /// Update Forward connections
        std::unique_lock lock(forward_listener_mutex);
        if (update_forward_listener)
            update_forward_listener();
    }
    return nuraft::cb_func::ReturnCode::Ok;
}

void KeeperServer::waitInit()
{
    std::unique_lock lock(initialized_mutex);
    int64_t timeout = settings->raft_settings->startup_timeout;
    if (!initialized_cv.wait_for(lock, std::chrono::milliseconds(timeout), [&] { return initialized_flag.load(); }))
        throw Exception(ErrorCodes::RAFT_ERROR, "Failed to wait RAFT initialization");
}

std::vector<int64_t> KeeperServer::getDeadSessions()
{
    return state_machine->getDeadSessions();
}

ConfigUpdateActions KeeperServer::getConfigurationDiff(const Poco::Util::AbstractConfiguration & config_)
{
    return state_manager->getConfigurationDiff(config_);
}

bool KeeperServer::applyConfigurationUpdate(const ConfigUpdateAction & task)
{
    size_t sleep_ms = 500;
    if (task.action_type == ConfigUpdateActionType::AddServer)
    {
        LOG_INFO(log, "Will try to add server with id {}", task.server->get_id());
        bool added = false;
        for (size_t i = 0; i < settings->raft_settings->configuration_change_tries_count; ++i)
        {
            if (raft_instance->get_srv_config(task.server->get_id()) != nullptr)
            {
                LOG_INFO(log, "Server with id {} was successfully added", task.server->get_id());
                added = true;
                break;
            }

            if (!isLeader())
            {
                LOG_INFO(log, "We are not leader anymore, will not try to add server {}", task.server->get_id());
                break;
            }

            auto result = raft_instance->add_srv(*task.server);
            if (!result->get_accepted())
                LOG_INFO(
                    log,
                    "Command to add server {} was not accepted for the {} time, will sleep for {} ms and retry",
                    task.server->get_id(),
                    i + 1,
                    sleep_ms * (i + 1));

            LOG_DEBUG(log, "Wait for apply action AddServer {} done for {} ms", task.server->get_id(), sleep_ms * (i + 1));
            std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms * (i + 1)));
        }
        if (!added)
            throw Exception(
                ErrorCodes::RAFT_ERROR,
                "Configuration change to add server (id {}) was not accepted by RAFT after all {} retries",
                task.server->get_id(),
                settings->raft_settings->configuration_change_tries_count);
    }
    else if (task.action_type == ConfigUpdateActionType::RemoveServer)
    {
        LOG_INFO(log, "Will try to remove server with id {}", task.server->get_id());

        bool removed = false;
        if (task.server->get_id() == state_manager->server_id())
        {
            LOG_INFO(
                log,
                "Trying to remove leader node (ourself), so will yield leadership and some other node (new leader) will try remove us. "
                "Probably you will have to run SYSTEM RELOAD CONFIG on the new leader node");

            raft_instance->yield_leadership();
            std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms * 5));
            return false;
        }

        for (size_t i = 0; i < settings->raft_settings->configuration_change_tries_count; ++i)
        {
            if (raft_instance->get_srv_config(task.server->get_id()) == nullptr)
            {
                LOG_INFO(log, "Server with id {} was successfully removed", task.server->get_id());
                removed = true;
                break;
            }

            if (!isLeader())
            {
                LOG_INFO(log, "We are not leader anymore, will not try to remove server {}", task.server->get_id());
                break;
            }

            auto result = raft_instance->remove_srv(task.server->get_id());
            if (!result->get_accepted())
                LOG_INFO(
                    log,
                    "Command to remove server {} was not accepted for the {} time, will sleep for {} ms and retry",
                    task.server->get_id(),
                    i + 1,
                    sleep_ms * (i + 1));

            LOG_DEBUG(log, "Wait for apply action RemoveServer {} done for {} ms", task.server->get_id(), sleep_ms * (i + 1));
            std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms * (i + 1)));
        }
        if (!removed)
            throw Exception(
                ErrorCodes::RAFT_ERROR,
                "Configuration change to remove server (id {}) was not accepted by RAFT after all {} retries",
                task.server->get_id(),
                settings->raft_settings->configuration_change_tries_count);
    }
    else if (task.action_type == ConfigUpdateActionType::UpdatePriority)
        raft_instance->set_priority(task.server->get_id(), task.server->get_priority());
    else
        LOG_WARNING(log, "Unknown configuration update type {}", static_cast<uint64_t>(task.action_type));

    return true;
}

bool KeeperServer::waitConfigurationUpdate(const ConfigUpdateAction & task)
{
    size_t sleep_ms = 500;
    if (task.action_type == ConfigUpdateActionType::AddServer)
    {
        LOG_INFO(log, "Will try to wait server with id {} to be added", task.server->get_id());
        for (size_t i = 0; i < settings->raft_settings->configuration_change_tries_count; ++i)
        {
            if (raft_instance->get_srv_config(task.server->get_id()) != nullptr)
            {
                LOG_INFO(log, "Server with id {} was successfully added by leader", task.server->get_id());
                return true;
            }

            if (isLeader())
            {
                LOG_INFO(log, "We are leader now, probably we will have to add server {}", task.server->get_id());
                return false;
            }

            LOG_DEBUG(log, "Wait for action AddServer {} done for {} ms", task.server->get_id(), sleep_ms * (i + 1));
            std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms * (i + 1)));
        }
        return false;
    }
    else if (task.action_type == ConfigUpdateActionType::RemoveServer)
    {
        LOG_INFO(log, "Will try to wait remove of server with id {}", task.server->get_id());

        for (size_t i = 0; i < settings->raft_settings->configuration_change_tries_count; ++i)
        {
            if (raft_instance->get_srv_config(task.server->get_id()) == nullptr)
            {
                LOG_INFO(log, "Server with id {} was successfully removed by leader", task.server->get_id());
                return true;
            }

            if (isLeader())
            {
                LOG_INFO(log, "We are leader now, probably we will have to remove server {}", task.server->get_id());
                return false;
            }

            LOG_DEBUG(log, "Wait for action RemoveServer {} done for {} ms", task.server->get_id(), sleep_ms * (i + 1));
            std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms * (i + 1)));
        }
        return false;
    }
    else if (task.action_type == ConfigUpdateActionType::UpdatePriority)
        return true;
    else
        LOG_WARNING(log, "Unknown configuration update type {}", static_cast<uint64_t>(task.action_type));
    return true;
}

uint64_t KeeperServer::createSnapshot()
{
    uint64_t log_idx = raft_instance->create_snapshot();
    if (log_idx != 0)
        LOG_INFO(log, "Snapshot creation scheduled with last committed log index {}.", log_idx);
    else
        LOG_WARNING(log, "Failed to schedule snapshot creation task.");
    return log_idx;
}

KeeperLogInfo KeeperServer::getKeeperLogInfo()
{
    KeeperLogInfo log_info;
    auto log_store = state_manager->load_log_store();
    if (log_store)
    {
        log_info.first_log_idx = log_store->start_index();
        log_info.first_log_term = log_store->term_at(log_info.first_log_idx);
    }

    if (raft_instance)
    {
        log_info.last_log_idx = raft_instance->get_last_log_idx();
        log_info.last_log_term = raft_instance->get_last_log_term();
        log_info.last_committed_log_idx = raft_instance->get_committed_log_idx();
        log_info.leader_committed_log_idx = raft_instance->get_leader_committed_log_idx();
        log_info.target_committed_log_idx = raft_instance->get_target_committed_log_idx();
        log_info.last_snapshot_idx = raft_instance->get_last_snapshot_idx();
    }

    return log_info;
}

bool KeeperServer::requestLeader()
{
    return isLeader() || raft_instance->request_leadership();
}

void KeeperServer::registerForWardListener(UpdateForwardListener forward_listener)
{
    std::unique_lock lock(forward_listener_mutex);
    update_forward_listener = forward_listener;
}

}
