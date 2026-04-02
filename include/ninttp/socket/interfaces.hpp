#pragma once

#include <cstddef>
#include "endpoints.hpp"

namespace ninttp
{

    template <typename EndpointT>
    class IBindable{
        public:
            virtual ~IBindable() noexcept = default;
            virtual void bind(const EndpointT& endpoint) = 0;
    };

    //mainly focused on tcp connections
    template <typename ConnectedSocketT>
    class IListener{
        public:
            virtual ~IListener() = default;
            virtual void listen(const int n) = 0;

            virtual ConnectedSocketT accept() = 0;

        protected:
            int backlog_ = 0;
    };

    template <typename EndpointT>
    class IConnectable{
        public:
            virtual ~IConnectable() = default;
            virtual void connect(const EndpointT& address) = 0;
    };

    //has IO stream capabilities (probably ConnectedSocketT is a type that implements this or IDatagramIO)
    class IStreamIO{
        public:
            virtual ~IStreamIO() noexcept = default;

            virtual size_t send(const char* buffer, size_t len) = 0;
            virtual size_t receive(char* buffer, size_t len) = 0;
    };

    template <typename EndpointT>
    struct receiveResult{
        size_t read;
        EndpointT fromEndpoint;
    };

    //has IO datagram capabilities (probably ConnectedSocketT is a type that implements this or IStreamIO)
    //this needs a rework bc a udp socket really can send and receive from anyone, no state associated. Fixing
    //a derived class instance to just the type you specify makes it so that you can only really send or receive from the
    //endpoint you specified when defining the variable. One way is to take in a IEndpoint * instead of a generic type,
    //which allows the user to pass in whatever endpoint it needs. 
    template <typename SendEndpointT, typename RecvEndpointT>
    class IDatagramIO{
        public:
            virtual ~IDatagramIO() noexcept = default;

            virtual size_t sendTo(const SendEndpointT& endpoint, const char* buf, size_t len) = 0;

            virtual size_t receiveFrom(char* buf, size_t len) = 0;
    };
} // namespace ninttp