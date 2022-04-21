#pragma once

#include <Poco/Net/ParallelSocketReactor.h>
#include <Poco/Net/StreamSocket.h>
#include <Poco/Net/ServerSocket.h>
#include <Poco/Environment.h>
#include <Poco/NObserver.h>
#include <Poco/SharedPtr.h>
#include <vector>

#include <Interpreters/Context.h>


using Poco::Net::Socket;
using Poco::Net::SocketReactor;
using Poco::Net::ServerSocket;
using Poco::Net::StreamSocket;
using Poco::NObserver;
using Poco::AutoPtr;


namespace DB {

    template <class ServiceHandler, class SR>
    class ParallelSocketAcceptor
    /// This class implements the Acceptor part of the Acceptor-Connector design pattern.
    /// Only the difference from single-threaded version is documented here, For full 
    /// description see Poco::Net::SocketAcceptor documentation.
    /// 
    /// This is a multi-threaded version of SocketAcceptor, it differs from the
    /// single-threaded version in number of reactors (defaulting to number of processors)
    /// that can be specified at construction time and is rotated in a round-robin fashion
    /// by event handler. See ParallelSocketAcceptor::onAccept and 
    /// ParallelSocketAcceptor::createServiceHandler documentation and implementation for 
    /// details.
    {
    public:
        using ParallelReactor = Poco::Net::ParallelSocketReactor<SR>;
        using Observer = Poco::Observer<ParallelSocketAcceptor, ReadableNotification>;

        explicit ParallelSocketAcceptor(Context & keeper_context_,
                                        ServerSocket& socket,
                                        unsigned threads = Poco::Environment::processorCount()):
            keeper_context(keeper_context_), socket_(socket), reactor_(nullptr), threads_(threads),
            next_(0)
        /// Creates a ParallelSocketAcceptor using the given ServerSocket, 
        /// sets number of threads and populates the reactors vector.
        {
            init();
        }

        ParallelSocketAcceptor(Context & keeper_context_,
                               ServerSocket& socket,
                               SocketReactor& reactor,
                               unsigned threads = Poco::Environment::processorCount()):
            socket_(socket), reactor_(&reactor), threads_(threads), keeper_context(keeper_context_),
            next_(0)
        /// Creates a ParallelSocketAcceptor using the given ServerSocket, sets the 
        /// number of threads, populates the reactors vector and registers itself 
        /// with the given SocketReactor.
        {
            init();
            reactor_->addEventHandler(socket_, Observer(*this, &ParallelSocketAcceptor::onAccept));
        }

        virtual ~ParallelSocketAcceptor()
        /// Destroys the ParallelSocketAcceptor.
        {
            try
            {
                if (reactor_)
                {
                    reactor_->removeEventHandler(socket_, Observer(*this, &ParallelSocketAcceptor::onAccept));
                }
            }
            catch (...)
            {
            }
        }

        void setReactor(SocketReactor& reactor)
        /// Sets the reactor for this acceptor.
        {
            registerAcceptor(reactor);
        }

        virtual void registerAcceptor(SocketReactor& reactor)
        /// Registers the ParallelSocketAcceptor with a SocketReactor.
        ///
        /// A subclass can override this function to e.g.
        /// register an event handler for timeout event.
        /// 
        /// The overriding method must either call the base class
        /// implementation or register the accept handler on its own.
        {
            reactor_ = &reactor;
            if (!reactor_->hasEventHandler(socket_, Observer(*this, &ParallelSocketAcceptor::onAccept)))
            {
                reactor_->addEventHandler(socket_, Observer(*this, &ParallelSocketAcceptor::onAccept));
            }
        }
	
        virtual void unregisterAcceptor()
        /// Unregisters the ParallelSocketAcceptor.
        ///
        /// A subclass can override this function to e.g.
        /// unregister its event handler for a timeout event.
        /// 
        /// The overriding method must either call the base class
        /// implementation or unregister the accept handler on its own.
        {
            if (reactor_)
            {
                reactor_->removeEventHandler(socket_, Observer(*this, &ParallelSocketAcceptor::onAccept));
            }
        }
	
        void onAccept(ReadableNotification* pNotification)
        /// Accepts connection and creates event handler.
        {
            pNotification->release();
            StreamSocket sock = socket_.acceptConnection();
            reactor_->wakeUp();
            createServiceHandler(sock);
        }

    protected:
        using ReactorVec = std::vector<typename ParallelReactor::Ptr>;

        virtual ServiceHandler* createServiceHandler(StreamSocket& socket)
        /// Create and initialize a new ServiceHandler instance.
        /// If socket is already registered with a reactor, the new
        /// ServiceHandler instance is given that reactor; otherwise,
        /// the next reactor is used. Reactors are rotated in round-robin
        /// fashion.
        ///
        /// Subclasses can override this method.
        {
            SocketReactor* pReactor = reactor(socket);
            if (!pReactor)
            {
                std::size_t next = next_++;
                if (next_ == reactors_.size()) next_ = 0;
                pReactor = reactors_[next];
            }
            pReactor->wakeUp();
            return new ServiceHandler(keeper_context, socket, *pReactor);
        }

        SocketReactor* reactor(const Socket& socket)
        /// Returns reactor where this socket is already registered
        /// for polling, if found; otherwise returns null pointer.
        {
            typename ReactorVec::iterator it = reactors_.begin();
            typename ReactorVec::iterator end = reactors_.end();
            for (; it != end; ++it)
            {
                if ((*it)->has(socket)) return it->get();
            }
            return nullptr;
        }

        SocketReactor* reactor()
        /// Returns a pointer to the SocketReactor where
        /// this SocketAcceptor is registered.
        ///
        /// The pointer may be null.
        {
            return reactor_;
        }

        Socket& socket()
        /// Returns a reference to the SocketAcceptor's socket.
        {
            return socket_;
        }

        void init()
        /// Populates the reactors vector.
        {
            poco_assert (threads_ > 0);

            for (unsigned i = 0; i < threads_; ++i)
                reactors_.push_back(new ParallelReactor);
        }

        ReactorVec& reactors()
        /// Returns reference to vector of reactors.
        {
            return reactors_;
        }

        SocketReactor* reactor(std::size_t idx)
        /// Returns reference to the reactor at position idx.
        {
            return reactors_.at(idx).get();
        }

        std::size_t next()
        /// Returns the next reactor index.
        {
            return next_;
        }

    private:
        ParallelSocketAcceptor() = delete;
        ParallelSocketAcceptor(const ParallelSocketAcceptor&) = delete;
        ParallelSocketAcceptor& operator = (const ParallelSocketAcceptor&) = delete;

        ServerSocket socket_;
        SocketReactor* reactor_;
        unsigned threads_;
        ReactorVec     reactors_;
        std::size_t    next_;

        Context & keeper_context;
    };


}
