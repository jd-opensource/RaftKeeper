#include "Server.h"
#include <memory>
#include <Core/Context.h>
#include <IO/UseSSL.h>
#include <Service/ConnectionHandler.h>
#include <Service/ForwardingConnectionHandler.h>
#include <Service/FourLetterCommand.h>
#include <Service/SvsSocketAcceptor.h>
#include <Service/SvsSocketReactor.h>
#include <Poco/Net/HTTPServer.h>
#include <Poco/Net/NetException.h>
#include <Poco/Util/HelpFormatter.h>
#include <Common/Config/ConfigReloader.h>
#include <Common/CurrentMetrics.h>
#include <Common/SensitiveDataMasker.h>
#include <Common/ThreadFuzzer.h>
#include <Common/ThreadProfileEvents.h>
#include <Common/ThreadStatus.h>
#include <Common/ZooKeeper/ZooKeeper.h>
#include <Common/ZooKeeper/ZooKeeperNodeCache.h>
#include <Common/config_version.h>
#include <Common/getExecutablePath.h>
#include <Common/getMappedArea.h>
#include <common/ErrorHandlers.h>

namespace RK
{
namespace ErrorCodes
{
    extern const int NO_ELEMENTS_IN_CONFIG;
    extern const int SUPPORT_IS_DISABLED;
    extern const int ARGUMENT_OUT_OF_BOUND;
    extern const int EXCESSIVE_ELEMENT_IN_CONFIG;
    extern const int INVALID_CONFIG_PARAMETER;
    extern const int SYSTEM_ERROR;
    extern const int FAILED_TO_GETPWUID;
    extern const int MISMATCHING_USERS_FOR_PROCESS_AND_DATA;
    extern const int NETWORK_ERROR;
}


void Server::initialize(Application & self)
{
    logger().information("starting up service server");
    BaseDaemon::initialize(self);
}

void Server::uninitialize()
{
    logger().information("shutting down service server");
    BaseDaemon::uninitialize();
}

void Server::createServer(const std::string & listen_host, int port, bool listen_try, CreateServerFunc && func) const
{
    try
    {
        func(port);
    }
    catch (const Poco::Exception &)
    {
        std::string message = "Listen [" + listen_host + "]:" + std::to_string(port) + " failed: " + getCurrentExceptionMessage(false);

        if (listen_try)
        {
            LOG_WARNING(
                &logger(),
                "{}. If it is an IPv6 or IPv4 address and your host has disabled IPv6 or IPv4, then consider to "
                "specify not disabled IPv4 or IPv6 address to listen in <listen_host> element of configuration "
                "file. Example for disabled IPv6: <listen_host>0.0.0.0</listen_host> ."
                " Example for disabled IPv4: <listen_host>::</listen_host>",
                message);
        }
        else
        {
            throw Exception{message, ErrorCodes::NETWORK_ERROR};
        }
    }
}

int Server::run()
{
    if (config().hasOption("help"))
    {
        Poco::Util::HelpFormatter help_formatter(Server::options());
        auto header_str = fmt::format(
            "{} [OPTION] [-- [ARG]...]\n"
            "positional arguments can be used to rewrite config.xml properties, for example, --http_port=8010",
            commandName());
        help_formatter.setHeader(header_str);
        help_formatter.format(std::cout);
        return 0;
    }

    if (config().hasOption("version"))
    {
        std::cout << DBMS_NAME << " server version " << VERSION_STRING << VERSION_OFFICIAL << "." << std::endl;
        return 0;
    }

    return Application::run(); // NOLINT
}


int Server::main(const std::vector<std::string> & /*args*/)
{
    static ServerErrorHandler error_handler;
    Poco::ErrorHandler::set(&error_handler);
    Poco::Logger * log = &logger();

    if (ThreadFuzzer::instance().isEffective())
        LOG_WARNING(log, "ThreadFuzzer is enabled. Application will run slowly and unstable.");

#if !defined(NDEBUG) || !defined(__OPTIMIZE__)
    LOG_WARNING(log, "Server was built in debug mode. It will work slowly.");
#endif

#if defined(SANITIZER)
    LOG_WARNING(log, "Server was built with sanitizer. It will work slowly.");
#endif

    /// Try to increase limit on number of open files.
    {
        rlimit rlim;
        if (getrlimit(RLIMIT_NOFILE, &rlim))
            throw Poco::Exception("Cannot getrlimit");

        if (rlim.rlim_cur == rlim.rlim_max)
        {
            LOG_DEBUG(log, "rlimit on number of file descriptors is {}", rlim.rlim_cur);
        }
        else
        {
            rlim_t old = rlim.rlim_cur;
            rlim.rlim_cur = config().getUInt("max_open_files", rlim.rlim_max);
            int rc = setrlimit(RLIMIT_NOFILE, &rlim);
            if (rc != 0)
                LOG_WARNING(
                    log,
                    "Cannot set max number of file descriptors to {}. Try to specify max_open_files according to your system limits. "
                    "error: {}",
                    rlim.rlim_cur,
                    strerror(errno));
            else
                LOG_DEBUG(log, "Set max number of file descriptors to {} (was {}).", rlim.rlim_cur, old);
        }
    }

    auto & global_context = Context::get();
    
    std::shared_ptr<SvsSocketReactor<SocketReactor>> nio_server;
    std::shared_ptr<SvsSocketAcceptor<ConnectionHandler, SocketReactor>> nio_server_acceptor;

    //get port from config
    std::string listen_host = config().getString("keeper.host", "0.0.0.0");
    bool listen_try = config().getBool("listen_try", false);

    //Init global thread pool
    GlobalThreadPool::initialize(config().getUInt("max_thread_pool_size", 10000));

    global_context.initializeDispatcher();
    FourLetterCommandFactory::registerCommands(*global_context.getDispatcher());

    const char * port_name = "keeper.port";
    createServer(listen_host, config().getInt(port_name, 8101), listen_try, [&](UInt16 port) {
        Poco::Net::ServerSocket socket(port);
        socket.setBlocking(false);

        Poco::Timespan timeout(
            global_context.getConfigRef().getUInt(
                "keeper.raft_settings.operation_timeout_ms", Coordination::DEFAULT_OPERATION_TIMEOUT_MS * 1000)
            * 1000);
        nio_server = std::make_shared<SvsSocketReactor<SocketReactor>>(timeout, "NIO-ACCEPTOR");
        /// TODO add io thread count to config
        nio_server_acceptor = std::make_shared<SvsSocketAcceptor<ConnectionHandler, SocketReactor>>(
            "NIO-HANDLER", global_context, socket, *nio_server, timeout);
        LOG_INFO(log, "Listening for connections on {}", socket.address().toString());
    });

    std::shared_ptr<SvsSocketReactor<SocketReactor>> nio_forwarding_server;
    std::shared_ptr<SvsSocketAcceptor<ForwardingConnectionHandler, SocketReactor>> nio_forwarding_server_acceptor;


    const char * forwarding_port_name = "keeper.forwarding_port";
    createServer(listen_host, config().getInt(forwarding_port_name, 8102), listen_try, [&](UInt16 port) {
        Poco::Net::ServerSocket socket(port);
        socket.setBlocking(false);

        Poco::Timespan timeout(
            global_context.getConfigRef().getUInt(
                "keeper.raft_settings.operation_timeout_ms", Coordination::DEFAULT_OPERATION_TIMEOUT_MS * 1000)
            * 1000);
        nio_forwarding_server = std::make_shared<SvsSocketReactor<SocketReactor>>(timeout, "NIO-ACCEPTOR");
        /// TODO add io thread count to config
        nio_forwarding_server_acceptor = std::make_shared<SvsSocketAcceptor<ForwardingConnectionHandler, SocketReactor>>(
            "NIO-HANDLER", global_context, socket, *nio_forwarding_server, timeout);
        LOG_INFO(log, "Listening for connections on {}", socket.address().toString());
    });

    zkutil::EventPtr unused_event = std::make_shared<Poco::Event>();
    zkutil::ZooKeeperNodeCache unused_cache([] { return nullptr; });

    auto main_config_reloader = std::make_unique<ConfigReloader>(
        config_path,
        "",
        config().getString("path", ""),
        std::move(unused_cache),
        unused_event,
        [&](ConfigurationPtr config, bool /* initial_loading */) {
            if (config->has("keeper"))
                global_context.updateServiceKeeperConfiguration(*config);
        },
        /* already_loaded = */ false); /// Reload it right now (initial loading)

    buildLoggers(config(), logger());
    main_config_reloader->start();
    LOG_INFO(log, "Ready for connections.");

    SCOPE_EXIT({
        LOG_DEBUG(log, "Received termination signal.");
        LOG_DEBUG(log, "Waiting for current connections to close.");

        main_config_reloader.reset();
        is_cancelled = true;

        /// shutdown dispatcher
        global_context.shutdownDispatcher();

        LOG_INFO(log, "Will shutdown forcefully.");
        _exit(Application::EXIT_OK);
    });

    // 4. Wait for termination
    waitForTerminationRequest();

    return Application::EXIT_OK;
}

void Server::defineOptions(Poco::Util::OptionSet & options)
{
    options.addOption(Poco::Util::Option("help", "h", "show help and exit").required(false).repeatable(false).binding("help"));
    options.addOption(Poco::Util::Option("version", "V", "show version and exit").required(false).repeatable(false).binding("version"));
    BaseDaemon::defineOptions(options);
}

}


int mainEntryRaftKeeperServer(int argc, char ** argv)
{
    try
    {
        RK::Server server;
        //master.init(argc, argv);
        return server.run(argc, argv);
    }
    catch (...)
    {
        std::cerr << RK::getCurrentExceptionMessage(true) << "\n";
        auto code = RK::getCurrentExceptionCode();
        return code ? code : 1;
    }
}
