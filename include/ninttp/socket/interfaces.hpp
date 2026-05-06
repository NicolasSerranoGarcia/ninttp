#pragma once

#include <cstddef>
#include <concepts>
#include <expected>
#include "endpoints.hpp"
#include "socket_error.hpp"

namespace ninttp
{

    template <class DerivedT, typename EndpointT>
    concept hasBindableImpl = requires(DerivedT& derived, const EndpointT& endpoint) {
        { derived.bindImpl(endpoint) } -> std::same_as<std::expected<void, socketError>>;
    };

    template <class DerivedT, typename EndpointT>
        requires hasBindableImpl<DerivedT, EndpointT>
    class Bindable{
        public:
            std::expected<void, socketError> bind(const EndpointT& endpoint){
                return static_cast<DerivedT&>(*this).bindImpl(endpoint);
            }
    };

    //mainly focused on tcp connections
    template <class DerivedT, typename ConnectedSocketT>
    concept hasListenerImpl = requires(DerivedT& derived, int backlog) {
        { derived.listenImpl(backlog) } -> std::same_as<std::expected<void, socketError>>;
        { derived.acceptImpl() } -> std::same_as<std::expected<ConnectedSocketT, socketError>>;
    };

    template <class DerivedT, typename ConnectedSocketT>
        requires hasListenerImpl<DerivedT, ConnectedSocketT>
    class Listener{
        public:
            std::expected<void, socketError> listen(const int n){
                backlog_ = n;
                return static_cast<DerivedT&>(*this).listenImpl(n);
            }

            std::expected<ConnectedSocketT, socketError> accept(){
                return static_cast<DerivedT&>(*this).acceptImpl();
            }

        protected:
            int backlog_ = 0;
    };

    template <class DerivedT, typename EndpointT>
    concept hasConnectableImpl = requires(DerivedT& derived, const EndpointT& endpoint) {
        { derived.connectImpl(endpoint) } -> std::same_as<std::expected<void, socketError>>;
    };

    template <class DerivedT, typename EndpointT>
        requires hasConnectableImpl<DerivedT, EndpointT>
    class Connectable{
        public:
            std::expected<void, socketError> connect(const EndpointT& address){
                return static_cast<DerivedT&>(*this).connectImpl(address);
            }
    };

    template <class DerivedT>
    concept hasIOStreamImpl = requires(DerivedT& derived, const char* sendBuffer, char* recvBuffer, size_t len) {
        { derived.sendImpl(sendBuffer, len) } -> std::same_as<std::expected<size_t, socketError>>;
        { derived.receiveImpl(recvBuffer, len) } -> std::same_as<std::expected<size_t, socketError>>;
    };
    
    //has IO stream capabilities
    template <class DerivedT>
        requires hasIOStreamImpl<DerivedT>
    class IOStream{
        public:
            std::expected<size_t, socketError> send(const char* buffer, size_t len){
                return static_cast<DerivedT&>(*this).sendImpl(buffer, len);
            }

            std::expected<size_t, socketError> receive(char* buffer, size_t len){
                return static_cast<DerivedT&>(*this).receiveImpl(buffer, len);
            }
    };

    //has IO datagram capabilities (probably ConnectedSocketT is a type that implements this or IOStream)
    //this needs a rework bc a udp socket really can send and receive from anyone, no state associated. Fixing
    //a derived class instance to just the type you specify makes it so that you can only really send or receive from the
    //endpoint you specified when defining the variable. One way is to take in a IEndpoint * instead of a generic type,
    //which allows the user to pass in whatever endpoint it needs. 
    template <class DerivedT, typename SendEndpointT, typename RecvEndpointT>
    concept hasIODatagramImpl = requires(DerivedT& derived, const SendEndpointT& endpoint, char* recvBuffer, const char* sendBuffer, size_t len) {
        { derived.sendToImpl(endpoint, sendBuffer, len) } -> std::same_as<size_t>;
        { derived.receiveFromImpl(recvBuffer, len) } -> std::same_as<size_t>;
    };

    template <class DerivedT, typename SendEndpointT, typename RecvEndpointT>
        requires hasIODatagramImpl<DerivedT, SendEndpointT, RecvEndpointT>
    class IODatagram{
        public:
            size_t sendTo(const SendEndpointT& endpoint, const char* buf, size_t len){
                return static_cast<DerivedT&>(*this).sendToImpl(endpoint, buf, len);
            }

            size_t receiveFrom(char* buf, size_t len){
                return static_cast<DerivedT&>(*this).receiveFromImpl(buf, len);
            }
    };
} // namespace ninttp