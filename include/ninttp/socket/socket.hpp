#pragma once

#include <concepts>
#include <expected>

#include "internal/select_backend.hpp"
#include "internal/socket_core.hpp"
#include "interfaces.hpp"
#include "types.hpp"

#include "socket_error.hpp"


namespace ninttp
{
    using SocketBase = ninttp::internal::SocketCore<internal::SelectedBackend>;

    template<typename ConnectedSocketT>
    concept ConnectedSocketFromAccepted = requires(
        typename internal::SelectedBackend::SocketT sock,
        Domain domain,
        Service service,
        Protocol proto,
        const typename internal::SelectedBackend::AddressStorageT& storage) {
        { ConnectedSocketT::fromAccepted(sock, domain, service, proto, storage) } noexcept
            -> std::convertible_to<std::expected<ConnectedSocketT, socketError>>;
    };

    template<typename EndpointT>
    concept EndpointFromStorage = requires(
        const typename internal::SelectedBackend::AddressStorageT& storage) {
        { EndpointT::fromStorage(storage) } -> std::convertible_to<EndpointT>;
    };

    template<typename EndpointT>
    concept EndpointToStorage = requires(const EndpointT& endpoint) {
        { endpoint.toStorage() } -> std::convertible_to<typename internal::SelectedBackend::AddressStorageT>;
        { endpoint.storageLen() } -> std::convertible_to<typename internal::SelectedBackend::AddressLenT>;
    };

    //connected socket needs a concept to check that it can be built fromAccepted (static method with the same signature)
    //EndpointT needs the signature ::fromStorage(const AddressStorageT& storage)
    //it also needs a toStorage and storageLen methods for being able to pass to the backend
    template<typename EndpointT, typename ConnectedSocketT>
        requires ConnectedSocketFromAccepted<ConnectedSocketT> && EndpointToStorage<EndpointT>
    class ListenerSocket : protected SocketBase,
                            public Bindable<ListenerSocket<EndpointT, ConnectedSocketT>, EndpointT>,
                            public Listener<ListenerSocket<EndpointT, ConnectedSocketT>, ConnectedSocketT>{
        public:
            //we should forward the methods that we do want to use from SocketBase, like getters, release...
            ListenerSocket(Domain domain, Protocol proto)
                : SocketBase(domain, Service::Stream, proto){}

            using SocketBase::close;
            using SocketBase::domain;
            using SocketBase::isOpen;
            using SocketBase::isUsable;
            using SocketBase::isValid;
            using SocketBase::nativeHandle;
            using SocketBase::protocol;
            using SocketBase::release;
            using SocketBase::service;
            using SocketBase::shutdown;

            std::expected<void, socketError> bind(const EndpointT& endpoint) noexcept{
                return bindImpl(endpoint);
            }

            std::expected<void, socketError> listen(int backlog) noexcept{
                return listenImpl(backlog);
            }

            std::expected<ConnectedSocketT, socketError> accept() noexcept{
                return acceptImpl();
            }

        private:
            template <class, typename>
            friend class Bindable;

            template <class, typename>
            friend class Listener;

            std::expected<void, socketError> bindImpl(const EndpointT& endpoint) noexcept{
                using AddressLenT = typename internal::SelectedBackend::AddressLenT;

                auto storage = endpoint.toStorage();
                auto len = static_cast<AddressLenT>(endpoint.storageLen());

                if(!internal::SelectedBackend::bind(handle_, &storage, len)) {
                    return std::unexpected{socketError{internal::SelectedBackend::getLastError()}};
                }

                return {};
            }

            std::expected<void, socketError> listenImpl(int backlog) noexcept{
                if(!internal::SelectedBackend::listen(handle_, backlog)) {
                    return std::unexpected{socketError{internal::SelectedBackend::getLastError()}};
                }

                return {};
            }

            std::expected<ConnectedSocketT, socketError> acceptImpl() noexcept{
                auto accepted = internal::SelectedBackend::accept(handle_);
                if(!accepted.has_value()) {
                    return std::unexpected{socketError{internal::SelectedBackend::getLastError()}};
                }

                return ConnectedSocketT::fromAccepted(
                    accepted->socket,
                    domain_,
                    service_,
                    proto_,
                    accepted->storage);
            }
    };

    template<typename EndpointT>
        requires EndpointFromStorage<EndpointT> && EndpointToStorage<EndpointT>
    class StreamSocket : protected SocketBase,
                         public Connectable<StreamSocket<EndpointT>, EndpointT>,
                         public IOStream<StreamSocket<EndpointT>>{
        public:
            StreamSocket(Domain domain, Protocol proto)
                : SocketBase(domain, Service::Stream, proto){}

            using SocketBase::close;
            using SocketBase::domain;
            using SocketBase::isOpen;
            using SocketBase::isUsable;
            using SocketBase::isValid;
            using SocketBase::nativeHandle;
            using SocketBase::protocol;
            using SocketBase::release;
            using SocketBase::service;
            using SocketBase::shutdown;

            static std::expected<StreamSocket, socketError> fromAccepted(
                typename internal::SelectedBackend::SocketT sock,
                Domain domain,
                Service service,
                Protocol proto,
                const typename internal::SelectedBackend::AddressStorageT& peerStorage) noexcept
            {
                return StreamSocket(sock, domain, service, proto, EndpointT::fromStorage(peerStorage));
            }

            std::expected<void, socketError> connect(const EndpointT& endpoint) noexcept{
                return connectImpl(endpoint);
            }

            std::expected<size_t, socketError> send(const char* buffer, size_t len) noexcept{
                return sendImpl(buffer, len);
            }

            std::expected<size_t, socketError> receive(char* buffer, size_t len) noexcept{
                return receiveImpl(buffer, len);
            }

        private:
            template <class, typename>
            friend class Connectable;

            template <class>
            friend class IOStream;

            std::expected<void, socketError> connectImpl(const EndpointT& endpoint) noexcept{
                using AddressLenT = typename internal::SelectedBackend::AddressLenT;

                auto storage = endpoint.toStorage();
                auto len = static_cast<AddressLenT>(endpoint.storageLen());

                if(!internal::SelectedBackend::connect(handle_, &storage, len)) {
                    return std::unexpected{socketError{internal::SelectedBackend::getLastError()}};
                }

                endpoint_ = endpoint;
                return {};
            }

            std::expected<size_t, socketError> sendImpl(const char* buffer, size_t n) noexcept{
                auto sent = internal::SelectedBackend::send(handle_, buffer, n);
                if(sent == -1) {
                    return std::unexpected{socketError{internal::SelectedBackend::getLastError()}};
                }

                return sent;
            }

            std::expected<size_t, socketError> receiveImpl(char* buffer, size_t n) noexcept{
                auto got = internal::SelectedBackend::receive(handle_, buffer, n);
                if(got == -1) {
                    return std::unexpected{socketError{internal::SelectedBackend::getLastError()}};
                }

                return got;
            }

            StreamSocket(
                typename internal::SelectedBackend::SocketT sock,
                Domain domain,
                Service service,
                Protocol proto,
                const EndpointT& endpoint) noexcept
                : SocketBase(sock, domain, service, proto), endpoint_(endpoint){}

            EndpointT endpoint_{};
    };

    #if NINTTP_SOCKET_BACKEND_REQUIRES_INIT == 1
    //this function is completely optional (and personally I would not use it) unless for very specific reasons. One of them might be
    //that you are on windows, using Winsock, and you are doing additional OS API level programming, and you need fine grained control 
    //over the availability of the winsock dll. Otherwise (your program needs ninttp until closing) it will get cleaned up and no resources will
    //be leaked
    std::expected<void, socketError> deinitBackend() noexcept{
        static bool deinited = false;

        if(deinited){
            return {};
        }

        if(internal::SelectedBackend::deinit()){
            deinited = true;
            return {};
        }

        return std::unexpected{socketError{internal::SelectedBackend::getLastError()}};
    }
    #endif
} // namespace ninttp
