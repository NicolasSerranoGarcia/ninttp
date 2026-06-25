#pragma once

#include <cassert>
#include <concepts>
#include <expected>
#include <utility>
#include <span>

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
    // - accept() needs ConnectedSocketT to provide an adoption constructor with
    //   the signature used below. If that constructor is private, the connected
    //   socket type must friend ListenerSocket.
    // - The connected socket must need the private no throwing constructor. Elsewise the accept method 
    // will call terminate if the constructor throws
    template<typename EndpointT, typename ConnectedSocketT>
    class ListenerSocket : protected SocketBase{

        //A concept is useless here because there is no beahivoral check, but rather expecting just two types of endpoints
        static_assert(std::same_as<EndpointT, IPv4Endpoint> || std::same_as<EndpointT, IPv6Endpoint>,
            "Listener socket only accepts IPv4 or IPv6 endpoints");

        public:
            //we should forward the methods that we do want to use from SocketBase, like getters, release...
            ListenerSocket(Domain domain, Protocol proto)
                : SocketBase(domain, Service::Stream, proto)
            {
                //TODO: maybe give a user option in the future to deactivate this, but for the fromStorage 
                //converters to work without problems we currently deactivate it
                if constexpr(std::same_as<EndpointT, IPv6Endpoint>)
                    internal::SelectedBackend::setOption(this->handle_, internal::SocketOption::IPv6Only);
            }

            //forward some of the methods to the public API
            using SocketBase::close;
            using SocketBase::domain;
            using SocketBase::isUsable;
            using SocketBase::protocol;
            using SocketBase::release;
            using SocketBase::service;
            using SocketBase::shutdown;

            std::expected<void, SocketError> bind(const EndpointT& endpoint) noexcept{
                using AddressLenT = typename internal::SelectedBackend::AddressLenT;

                auto storage = internal::SelectedBackend::toStorage(endpoint);
                auto len = static_cast<AddressLenT>(internal::SelectedBackend::storageLen(endpoint));

                auto bound = internal::SelectedBackend::bind(handle_, &storage, len);
                if(!bound.has_value())
                    return std::unexpected{SocketError{bound.error()}};

                return {};
            }

            std::expected<void, SocketError> listen(int backlog) noexcept{
                auto listening = internal::SelectedBackend::listen(handle_, backlog);
                if(!listening.has_value())
                    return std::unexpected{SocketError{listening.error()}};

                return {};
            }

            std::expected<ConnectedSocketT, SocketError> accept() noexcept{
                auto accepted = internal::SelectedBackend::accept(handle_);
                if(!accepted.has_value())
                    return std::unexpected{SocketError{accepted.error()}};

                //this is a type defined by us so we must guarantee no error whatsoever
                auto endpoint = internal::SelectedBackend::template fromStorageUnchecked<EndpointT>(accepted->storage);

                return ConnectedSocketT(
                    accepted->socket,
                    domain_,
                    service_,
                    proto_,
                    endpoint);
            }
    };

    // StreamSocket follows this contract:
    // - connect() needs the selected backend to translate EndpointT to native storage.
    // - accept() needs the selected backend to rebuild EndpointT from native storage.
    template<typename EndpointT>
    class StreamSocket : protected SocketBase{
        static_assert(std::same_as<EndpointT, IPv4Endpoint> || std::same_as<EndpointT, IPv6Endpoint>,
            "Stream socket only accepts IPv4 or IPv6 endpoints");

        // Connected socket extensions with a private adoption constructor must
        // grant ListenerSocket the same friendship to be accepted by it.
        template<typename, typename>
        friend class ListenerSocket;

        public:
            StreamSocket(Domain domain, Protocol proto)
                : SocketBase(domain, Service::Stream, proto){}

            using SocketBase::close;
            using SocketBase::domain;
            using SocketBase::isUsable;
            using SocketBase::protocol;
            using SocketBase::release;
            using SocketBase::service;
            using SocketBase::shutdown;

            std::expected<void, SocketError> connect(const EndpointT& endpoint) noexcept{
                using AddressLenT = typename internal::SelectedBackend::AddressLenT;

                auto storage = internal::SelectedBackend::toStorage(endpoint);
                auto len = static_cast<AddressLenT>(internal::SelectedBackend::storageLen(endpoint));

                auto connected = internal::SelectedBackend::connect(handle_, &storage, len);
                if(!connected.has_value())
                    return std::unexpected{SocketError{connected.error()}};

                peerEndpoint_ = endpoint;
                return {};
            }

            //concurrent use of a send function while closing the socket is currently not
            //supported and strongly disallowed as it can lead to fatal connection crashes.
            std::expected<size_t, SocketError> send(std::span<const char> data) noexcept{
                auto sent = internal::SelectedBackend::send(handle_, data);
                if(!sent.has_value())
                    return std::unexpected{SocketError{sent.error()}};

                return *sent;
            }

            //concurrent use of a send function while closing the socket is currently not
            //supported and strongly disallowed as it can lead to fatal connection crashes.
            std::expected<size_t, SocketError> receive(std::span<char> buffer) noexcept{
                auto got = internal::SelectedBackend::receive(handle_, buffer);
                if(!got.has_value())
                    return std::unexpected{SocketError{got.error()}};

                return *got;
            }

        private:
            StreamSocket(
                typename internal::SelectedBackend::SocketT sock,
                Domain domain,
                Service service,
                Protocol proto,
                const EndpointT& endpoint) noexcept
                    : SocketBase(sock, domain, service, proto), peerEndpoint_(endpoint)
                {}

            EndpointT peerEndpoint_{};
    };
} // namespace ninttp
