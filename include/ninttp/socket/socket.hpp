#pragma once

#include <concepts>
#include <expected>
#include <utility>

#include "internal/select_backend.hpp"
#include "internal/socket_core.hpp"
#include "types.hpp"

#include "socket_error.hpp"


namespace ninttp
{
    using SocketBase = ninttp::internal::SocketCore<internal::SelectedBackend>;

    // ListenerSocket intentionally has no class-level requires clause. The useful
    // requirements belong to the operations:
    // - bind() needs the selected backend to translate EndpointT to native storage.
    // - accept() needs ConnectedSocketT::fromAccepted(...).
    template<typename EndpointT, typename ConnectedSocketT>
    class ListenerSocket : protected SocketBase{
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
                using AddressLenT = typename internal::SelectedBackend::AddressLenT;

                auto storage = internal::SelectedBackend::toStorage(endpoint);
                auto len = static_cast<AddressLenT>(internal::SelectedBackend::storageLen(endpoint));

                if(!internal::SelectedBackend::bind(handle_, &storage, len)) {
                    return std::unexpected{socketError{internal::SelectedBackend::getLastError()}};
                }

                return {};
            }

            std::expected<void, socketError> listen(int backlog) noexcept{
                if(!internal::SelectedBackend::listen(handle_, backlog)) {
                    return std::unexpected{socketError{internal::SelectedBackend::getLastError()}};
                }

                return {};
            }

            std::expected<ConnectedSocketT, socketError> accept(){
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

    // StreamSocket follows the same "method contract" rule:
    // - connect() needs the selected backend to translate EndpointT to native storage.
    // - fromAccepted() needs the selected backend to rebuild EndpointT from native storage.
    template<typename EndpointT>
    class StreamSocket : protected SocketBase{
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
                const typename internal::SelectedBackend::AddressStorageT& peerStorage)
            {
                return StreamSocket(
                    sock,
                    domain,
                    service,
                    proto,
                    internal::SelectedBackend::template fromStorage<EndpointT>(peerStorage));
            }

            std::expected<void, socketError> connect(const EndpointT& endpoint) noexcept{
                using AddressLenT = typename internal::SelectedBackend::AddressLenT;

                auto storage = internal::SelectedBackend::toStorage(endpoint);
                auto len = static_cast<AddressLenT>(internal::SelectedBackend::storageLen(endpoint));

                if(!internal::SelectedBackend::connect(handle_, &storage, len)) {
                    return std::unexpected{socketError{internal::SelectedBackend::getLastError()}};
                }

                endpoint_ = endpoint;
                return {};
            }

            std::expected<size_t, socketError> send(const char* buffer, size_t len) noexcept{
                auto sent = internal::SelectedBackend::send(handle_, buffer, len);
                if(sent == -1) {
                    return std::unexpected{socketError{internal::SelectedBackend::getLastError()}};
                }

                return sent;
            }

            std::expected<size_t, socketError> receive(char* buffer, size_t len) noexcept{
                auto got = internal::SelectedBackend::receive(handle_, buffer, len);
                if(got == -1) {
                    return std::unexpected{socketError{internal::SelectedBackend::getLastError()}};
                }

                return got;
            }

        private:
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

        if(deinited)
            return {};

        if(internal::SelectedBackend::deinit()){
            deinited = true;
            return {};
        }

        return std::unexpected{socketError{internal::SelectedBackend::getLastError()}};
    }
    #endif
} // namespace ninttp
