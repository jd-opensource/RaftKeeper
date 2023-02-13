/**
 * Copyright 2021-2023 JD.com, Inc.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *         http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <Service/KeeperDispatcher.h>
#include <Service/RequestAccumulator.h>

namespace RK
{

void RequestAccumulator::push(RequestForSession request_for_session)
{
    requests_queue->push(request_for_session);
}


void RequestAccumulator::run(RunnerId runner_id)
{
    NuRaftResult result = nullptr;
    /// Requests from previous iteration. We store them to be able
    /// to send errors to the client.
    //    KeeperStore::RequestsForSessions prev_batch;

    KeeperStore::RequestsForSessions to_append_batch;
    UInt64 max_wait = operation_timeout_ms;

    while (!shutdown_called)
    {
        KeeperStore::RequestForSession request_for_session;

        bool pop_succ = false;
        if (to_append_batch.empty())
        {
            pop_succ = requests_queue->tryPop(runner_id, request_for_session, std::min(static_cast<uint64_t>(1000), max_wait));
        }
        else
        {
            if (!requests_queue->tryPop(runner_id, request_for_session))
            {
                result = server->putRequestBatch(to_append_batch);
                waitResultAndHandleError(result, to_append_batch);
                result.reset();
                to_append_batch.clear();
                continue;
            }
            pop_succ = true;
        }

        if (pop_succ)
        {
            to_append_batch.emplace_back(request_for_session);

            if (to_append_batch.size() >= max_batch_size)
            {
                result = server->putRequestBatch(to_append_batch);
                waitResultAndHandleError(result, to_append_batch);
                result.reset();
                to_append_batch.clear();
            }
        }
    }
}

bool RequestAccumulator::waitResultAndHandleError(NuRaftResult prev_result, const KeeperStore::RequestsForSessions & prev_batch)
{
    /// Forcefully process all previous pending requests

    if (!prev_result->has_result())
        prev_result->get();

    bool result_accepted = prev_result->get_accepted();

    for (const auto & request_session : prev_batch)
    {
        if (request_session.isForwardRequest())
        {
            ForwardResponse response{
                Result,
                result_accepted,
                prev_result->get_result_code(),
                request_session.session_id,
                request_session.request->xid,
                request_session.request->getOpNum()};
            keeper_dispatcher->sendAppendEntryResponse(request_session.server_id, request_session.client_id, response);
        }
        else if (!result_accepted || prev_result->get_result_code() != nuraft::cmd_result_code::OK)
        {
            request_processor->onError(
                result_accepted,
                prev_result->get_result_code(),
                request_session.session_id,
                request_session.request->xid,
                request_session.request->getOpNum());
        }
    }

    return result_accepted && prev_result->get_result_code() == nuraft::cmd_result_code::OK;
}

void RequestAccumulator::shutdown()
{
    if (shutdown_called)
        return;

    shutdown_called = true;

    KeeperStore::RequestForSession request_for_session;
    while (requests_queue->tryPopAny(request_for_session))
    {
        request_processor->onError(
            false,
            nuraft::cmd_result_code::CANCELLED,
            request_for_session.session_id,
            request_for_session.request->xid,
            request_for_session.request->getOpNum());
    }
}

void RequestAccumulator::initialize(
    size_t thread_count,
    std::shared_ptr<KeeperDispatcher> keeper_dispatcher_,
    std::shared_ptr<KeeperServer> server_,
    UInt64 operation_timeout_ms_,
    UInt64 max_batch_size_)
{
    keeper_dispatcher = keeper_dispatcher_;
    operation_timeout_ms = operation_timeout_ms_;
    max_batch_size = max_batch_size_;
    server = server_;
    requests_queue = std::make_shared<RequestsQueue>(thread_count, 20000);
    request_thread = std::make_shared<ThreadPool>(thread_count);
    for (size_t i = 0; i < thread_count; i++)
    {
        request_thread->trySchedule([this, i] { run(i); });
    }
}

}
