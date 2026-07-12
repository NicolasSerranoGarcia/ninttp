/**
 * @file socket.hpp
 * @author Nicolás Serrano (serranogarcianicolas@gmail.com)
 * @brief the main socket utility file that serves as an umbrella for other files
 * @version 0.1
 * @date 2026-06-26
 * 
 * @copyright Copyright (c) 2026 Nicolás Serrano García
 * 
 */

#pragma once

#include <cassert>
#include <concepts>
#include <cstddef>
#include <expected>
#include <span>
#include <string>
#include <string_view>

#include "internal/select_backend.hpp"
#include "internal/socket_core.hpp"
#include "concepts.hpp"
#include "traits.hpp"

#include "error/socket_error.hpp"

namespace ninttp
{
    /**
     * @brief Socket core specialized with the backend selected for this platform.
     */
    using SocketBase = ninttp::internal::SocketCore<internal::SelectedBackend>;

    /**
     * @brief Stream listener socket bound to one endpoint family.
     *
     * ListenerSocket intentionally has no class-level requires clause. The useful
     * requirements belong to the operations:
     * - bind() needs the selected backend to translate EndpointT to native storage.
     * - accept() needs ConnectedSocketT to provide a noexcept adoption constructor
     *   with the signature used below. If that constructor is private, the connected
     *   socket type must friend ListenerSocket.
     */
    template<typename EndpointT, typename ConnectedSocketT>
    class ListenerSocket : protected SocketBase{

        //A concept is useless here because there is no beahivoral check, but rather expecting just two types of endpoints
        static_assert(std::same_as<EndpointT, IPv4Endpoint> || std::same_as<EndpointT, IPv6Endpoint>,
            "Listener socket only accepts IPv4 or IPv6 endpoints");
        static_assert(concepts::AcceptedConnectedSocket<ConnectedSocketT, EndpointT>,
            "Connected socket must provide a noexcept adoption constructor for accepted sockets");

        public:
            /**
             * @brief Construct and open a listener socket for the endpoint family.
             *
             * @throws SocketError If backend initialization fails, if opening the native
             * socket fails or if setting the option Ipv6Only for the Ipv6 version of the socket fails.
             */
            ListenerSocket(Protocol proto)
                : SocketBase(concepts::endpointDomain<EndpointT>, Service::Stream, proto)
            {
                //TODO: maybe give a user option in the future to deactivate this, but for the fromStorage 
                //converters to work without problems we currently deactivate it
                if constexpr(std::same_as<EndpointT, IPv6Endpoint>){
                    auto changed = internal::SelectedBackend::setOption(this->handle_, internal::SocketOption::IPv6Only);
                    if(!changed)
                        throw SocketError{changed.error()};
                }
            }

            using SocketBase::close;
            using SocketBase::domain;
            using SocketBase::isUsable;
            using SocketBase::protocol;
            using SocketBase::release;
            using SocketBase::service;
            using SocketBase::shutdown;

            /**
             * @brief Binds this listener to a local endpoint.
             *
             * @param endpoint Local address and port to bind.
             * @return Empty result on success, or SocketError wrapping the native bind error.
             */
            [[nodiscard]] std::expected<void, SocketError> bind(const EndpointT& endpoint) noexcept{
                using AddressLenT = typename internal::SelectedBackend::AddressLenT;

                auto storage = internal::SelectedBackend::toStorage(endpoint);
                auto len = static_cast<AddressLenT>(internal::SelectedBackend::storageLen(endpoint));

                auto bound = internal::SelectedBackend::bind(handle_, &storage, len);
                if(!bound.has_value())
                    return std::unexpected{SocketError{bound.error()}};

                return {};
            }

            /**
             * @brief Marks the bound socket as a passive listener.
             *
             * @param backlog Native backend backlog hint.
             * @return Empty result on success, or SocketError wrapping the native listen error.
             */
            [[nodiscard]] std::expected<void, SocketError> listen(int backlog) noexcept{
                auto listening = internal::SelectedBackend::listen(handle_, backlog);
                if(!listening.has_value())
                    return std::unexpected{SocketError{listening.error()}};

                return {};
            }

            /**
             * @brief Accepts one pending connection.
             *
             * The accepted endpoint is reconstructed from backend-provided native storage. This
             * function uses the unchecked conversion because the storage comes directly from a
             * successful backend accept call.
             *
             * @return Connected socket on success, or SocketError wrapping the native accept
             * error.
             */
            [[nodiscard]] std::expected<ConnectedSocketT, SocketError> accept() noexcept{
                auto accepted = internal::SelectedBackend::accept(handle_);
                if(!accepted.has_value())
                    return std::unexpected{SocketError{accepted.error()}};

                auto endpoint = internal::SelectedBackend::template fromStorageUnchecked<EndpointT>(accepted->storage);

                return ConnectedSocketT(
                    accepted->socket,
                    domain_,
                    service_,
                    proto_,
                    endpoint);
            }
    };

    /**
     * @brief Connected stream socket bound to one endpoint family.
     *
     * connect() needs the selected backend to translate EndpointT to native storage.
     * Accepted sockets are built through ListenerSocket using the private adoption constructor.
     */
    template<typename EndpointT>
    class StreamSocket : protected SocketBase{
        static_assert(std::same_as<EndpointT, IPv4Endpoint> || std::same_as<EndpointT, IPv6Endpoint>,
            "Stream socket only accepts IPv4 or IPv6 endpoints");

        // Connected socket extensions with a private adoption constructor must
        // grant ListenerSocket the same friendship to be accepted by it.
        template<typename, typename>
        friend class ListenerSocket;

        public:
            /**
             * @brief Construct and open a stream socket for the endpoint family.
             *
             * @throws SocketError If backend initialization fails, or if opening the native
             * socket fails.
             */
            StreamSocket(Protocol proto)
                : SocketBase(concepts::endpointDomain<EndpointT>, Service::Stream, proto){}

            using SocketBase::close;
            using SocketBase::domain;
            using SocketBase::isUsable;
            using SocketBase::protocol;
            using SocketBase::release;
            using SocketBase::service;
            using SocketBase::shutdown;

            /**
             * @brief Connects this socket to a remote endpoint.
             *
             * @param endpoint Remote address and port.
             * @return Empty result on success, or SocketError wrapping the native connect error.
             */
            [[nodiscard]] std::expected<void, SocketError> connect(const EndpointT& endpoint) noexcept{
                using AddressLenT = typename internal::SelectedBackend::AddressLenT;

                auto storage = internal::SelectedBackend::toStorage(endpoint);
                auto len = static_cast<AddressLenT>(internal::SelectedBackend::storageLen(endpoint));

                auto connected = internal::SelectedBackend::connect(handle_, &storage, len);
                if(!connected.has_value())
                    return std::unexpected{SocketError{connected.error()}};

                peerEndpoint_ = endpoint;
                return {};
            }

            /**
             * @brief Sends bytes through the socket once.
             *
             * A successful native send may write fewer bytes than requested. Use sendAll() when
             * the whole span must be written before returning.
             *
             * Concurrent use of send while closing the same socket is not supported.
             *
             * @param data Bytes to send.
             * @return Number of bytes sent, or SocketError wrapping the native send error.
             */
            [[nodiscard]] std::expected<std::size_t, SocketError> send(std::span<const char> data) noexcept{
                auto sent = internal::SelectedBackend::send(handle_, data);
                if(!sent.has_value())
                    return std::unexpected{SocketError{sent.error()}};

                return *sent;
            }

            /**
             * @brief Sends all bytes in the provided span.
             *
             * The loop retries from the first unsent byte after partial writes. For non-empty
             * spans, the backend send invariant is that success reports forward progress; that
             * invariant is asserted.
             *
             * Concurrent use of sendAll while closing the same socket is not supported.
             *
             * @param data Bytes to send.
             * @return Empty result on success, or SocketError wrapping the native send error.
             */
            [[nodiscard]] std::expected<void, SocketError> sendAll(std::span<const char> data) noexcept{
                while(!data.empty()){
                    auto sent = this->send(data);
                    if(!sent.has_value())
                        return std::unexpected{sent.error()};

                    assert(*sent != 0);

                    data = data.subspan(*sent);
                }

                return {};
            }

            /**
             * @brief Sends every character in a string view.
             *
             * @param data Characters to send.
             * @return Empty result on success, or SocketError wrapping the native send error.
             */
            [[nodiscard]] std::expected<void, SocketError> sendAll(std::string_view data) noexcept{
                return sendAll(std::span<const char>{data.data(), data.size()});
            }

            [[nodiscard]] std::expected<void, SocketError> sendAll(const std::string& data) noexcept{
                return sendAll(std::string_view{data});
            }

            /**
             * @brief Receives bytes from the socket once.
             *
             * A successful receive can return fewer bytes than the buffer size. A returned byte
             * count of zero represents the native backend's end-of-stream condition.
             *
             * Concurrent use of receive while closing the same socket is not supported.
             *
             * @param buffer Destination buffer for received bytes.
             * @return Number of bytes received, or SocketError wrapping the native receive error.
             */
            [[nodiscard]] std::expected<std::size_t, SocketError> receive(std::span<char> buffer) noexcept{
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
