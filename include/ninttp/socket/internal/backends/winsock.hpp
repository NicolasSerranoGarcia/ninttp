/**
 * @file winsock.hpp
 * @author Nicolas Serrano (serranogarcianicolas@gmail.com)
 * @brief Defines the Winsock socket backend implementation.
 * @version 0.1
 * @date 2026-06-26
 *
 * @copyright Copyright (c) 2026 Nicolas Serrano Garcia
 *
 */

#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>

#include <cassert>
#include <concepts>
#include <cstring>
#include <expected>
#include <limits>
#include <optional>
#include <span>
#include <string_view>
#include <string>

#include "../../types.hpp"
#include "../../utils.hpp"
#include "../../../endpoints.hpp"
#include "concepts.hpp"

namespace ninttp::internal
{

    static constexpr inline int toNative(Domain domain) noexcept{
        switch (domain) {
            case Domain::IPv4: return AF_INET;
            case Domain::IPv6: return AF_INET6;
            default: return -1;
        }
    }
    
    static constexpr inline int toNative(Service service) noexcept{
        switch (service) {
            case Service::Stream: return SOCK_STREAM;
            case Service::Datagram: return SOCK_DGRAM;
            case Service::Raw: return SOCK_RAW;
            case Service::SeqPacket: return SOCK_SEQPACKET;
            default: return -1;
        }
    }
    
    static constexpr inline int toNative(Protocol protocol) noexcept{
        switch (protocol) {
            case Protocol::Default: return 0;
            case Protocol::Tcp: return IPPROTO_TCP;
            case Protocol::Udp: return IPPROTO_UDP;
            default: return -1;
        }
    }
    
    static constexpr inline int toNativeShutdownPolicy(ShutdownPolicy what) noexcept{
        switch(what){
            case ShutdownPolicy::SHUT_RECEPTIONS:
                return SD_RECEIVE;
            case ShutdownPolicy::SHUT_TRANSMISSIONS:
                return SD_SEND;
            case ShutdownPolicy::SHUT_TRANSMISSIONS_AND_RECEPTIONS:
                return SD_BOTH;
            default:
                return -1;
        }
    }


    /**
     * @brief Winsock implementation of the SocketBackend contract.
     */
    class WinsockBackend{
        public:
            using ErrorT = int;
            using SocketT = SOCKET;
            using AddressStorageT = sockaddr_storage;
            using AddressLenT = int; //included from the own winsock2
            using CloseStatusT = SocketCloseStatus<ErrorT>;

            struct AddressBundleT{
                SocketT socket;
                AddressStorageT storage;
                AddressLenT len;
            };

            /**
             * @brief Initializes Winsock for socket operations.
             *
             * SocketCore guards this call with std::call_once, so ninttp performs only one
             * successful backend initialization attempt in a process.
             *
             * @return Empty result on success, or the WSAStartup error code.
             */
            static std::expected<void, ErrorT> init() noexcept{
                WSADATA wsaData;

                const int result = WSAStartup(MAKEWORD(2,2), &wsaData);
                if(result != 0)
                    return std::unexpected{static_cast<ErrorT>(result)};

                return {};
            }

            /**
             * @brief Deinitializes Winsock for ninttp socket operations.
             *
             * Callers must respect deinitBackend()'s lifetime contract: no live ninttp sockets,
             * no concurrent deinitialization, and no later ninttp reinitialization attempt.
             *
             * @return Empty result on success, or the WSACleanup error code.
             */
            static std::expected<void, ErrorT> deinit() noexcept{
                if(WSACleanup() == SOCKET_ERROR)
                    return std::unexpected{getLastError()};

                return {};
            }

            /**
             * @brief Opens a native Winsock socket.
             *
             * @return Native socket on success, or WSAGetLastError() from socket().
             */
            static std::expected<SocketT, ErrorT> openSocket(Domain d, Service s, Protocol p) noexcept{
                const SocketT socket = static_cast<SocketT>(::socket(toNative(d), toNative(s), toNative(p)));
                if(socket == invalidSocket())
                    return std::unexpected{getLastError()};

                return socket;
            }
            
            /**
             * @brief Closes a native Winsock socket.
             *
             * @return Ownership disposition and optional WSA error from closesocket().
             */
            static CloseStatusT closeSocket(const SocketT& s) noexcept{
                if(::closesocket(s) == 0)
                    return {SocketCloseDisposition::Released, std::nullopt};

                const ErrorT error = getLastError();

                if(error == WSAEWOULDBLOCK)
                    return {SocketCloseDisposition::Retry, error};

                if(error == WSAENOTSOCK)
                    return {SocketCloseDisposition::Released, error};

                return {SocketCloseDisposition::Unspecified, error};
            };
            
            /**
             * @brief Checks whether a socket is not the backend invalid sentinel.
             */
            static constexpr bool isUsableSocket(const SocketT& s) noexcept{ return s != invalidSocket(); };
            
            /**
             * @brief Applies native shutdown() to the socket.
             *
             * @return Empty result on success, or WSAGetLastError() from shutdown().
             */
            static std::expected<void, ErrorT> shutdownSocket(const SocketT& s, ShutdownPolicy what) noexcept{
                if(::shutdown(s, toNativeShutdownPolicy(what)) == SOCKET_ERROR)
                    return std::unexpected{getLastError()};

                return {};
            };

            /**
             * @brief Enables a supported socket option.
             *
             * @return Empty result on success, WSAEOPNOTSUPP for unknown options, or the native
             * setsockopt() error.
             */
            static std::expected<void, ErrorT> setOption(const SocketT& s, SocketOption option) noexcept{
                switch(option){
                    case SocketOption::IPv6Only: {
                        const DWORD nativeValue = 1u;
                        if(::setsockopt(
                            s,
                            IPPROTO_IPV6,
                            IPV6_V6ONLY,
                            reinterpret_cast<const char*>(&nativeValue),
                            static_cast<int>(sizeof(nativeValue))) == SOCKET_ERROR)
                            return std::unexpected{getLastError()};

                        return {};
                    }
                }

                return std::unexpected{WSAEOPNOTSUPP};
            }

            /**
             * @brief Disables a supported socket option.
             *
             * @return Empty result on success, WSAEOPNOTSUPP for unknown options, or the native
             * setsockopt() error.
             */
            static std::expected<void, ErrorT> unsetOption(const SocketT& s, SocketOption option) noexcept{
                switch(option){
                    case SocketOption::IPv6Only: {
                        const DWORD nativeValue = 0u;
                        if(::setsockopt(
                            s,
                            IPPROTO_IPV6,
                            IPV6_V6ONLY,
                            reinterpret_cast<const char*>(&nativeValue),
                            static_cast<int>(sizeof(nativeValue))) == SOCKET_ERROR)
                            return std::unexpected{getLastError()};

                        return {};
                    }
                }

                return std::unexpected{WSAEOPNOTSUPP};
            }

            /**
             * @brief Binds a native socket to a native address.
             *
             * @return Empty result on success, or WSAGetLastError() from bind().
             */
            static std::expected<void, ErrorT> bind(const SocketT& s, const AddressStorageT* addr, AddressLenT len) noexcept{
                if(::bind(s, reinterpret_cast<const sockaddr*>(addr), len) == SOCKET_ERROR)
                    return std::unexpected{getLastError()};

                return {};
            }


            /**
             * @brief Marks a bound native socket as a listener.
             *
             * @return Empty result on success, or WSAGetLastError() from listen().
             */
            static std::expected<void, ErrorT> listen(const SocketT& s, int backlog) noexcept{
                if(::listen(s, backlog) == SOCKET_ERROR)
                    return std::unexpected{getLastError()};

                return {};
            }

            /**
             * @brief Accepts a native Winsock connection.
             *
             * @return Accepted socket/address bundle, or WSAGetLastError() from accept().
             */
            static std::expected<AddressBundleT, ErrorT> accept(const SocketT& s) noexcept{
                AddressStorageT storage{};
                AddressLenT len = sizeof(storage);

                SocketT sock = ::accept(s, reinterpret_cast<sockaddr*>(&storage), &len);

                if(sock == invalidSocket())
                    return std::unexpected{getLastError()};

                return AddressBundleT{sock, storage, len};
            }

            /**
             * @brief Connects a native socket to a native address.
             *
             * @return Empty result on success, or WSAGetLastError() from connect().
             */
            static std::expected<void, ErrorT> connect(const SocketT& s, const AddressStorageT* addr, AddressLenT len) noexcept{
                if(::connect(s, reinterpret_cast<const sockaddr*>(addr), len) == SOCKET_ERROR)
                    return std::unexpected{getLastError()};

                return {};
            }

            /**
             * @brief Converts an IPv4 endpoint to Winsock address storage.
             */
            static AddressStorageT toStorage(const IPv4Endpoint& endpoint) noexcept{ 
                sockaddr_in native{};
                native.sin_family = AF_INET;
                native.sin_addr.s_addr = hostToNetwork32(endpoint.addressHostOrder());
                native.sin_port = hostToNetwork16(endpoint.portHostOrder());

                AddressStorageT storage{};
                std::memcpy(&storage, &native, sizeof(native));
                return storage;
            }

            static constexpr AddressLenT storageLen(const IPv4Endpoint&) noexcept{
                return static_cast<AddressLenT>(sizeof(sockaddr_in));
            }

            /**
             * @brief Converts an IPv6 endpoint to Winsock address storage.
             */
            static AddressStorageT toStorage(const IPv6Endpoint& endpoint) noexcept{
                sockaddr_in6 native{};
                native.sin6_family = AF_INET6;
                native.sin6_port = hostToNetwork16(endpoint.portHostOrder());

                const auto bytes = endpoint.addressBytes();
                std::memcpy(&native.sin6_addr.s6_addr, bytes.data(), bytes.size());

                AddressStorageT storage{};
                std::memcpy(&storage, &native, sizeof(native));
                return storage;
            }

            static constexpr AddressLenT storageLen(const IPv6Endpoint&) noexcept{
                return static_cast<AddressLenT>(sizeof(sockaddr_in6));
            }

            /**
             * @brief Converts native storage to an endpoint after checking the address family.
             *
             * @return Endpoint on success, or WSAEAFNOSUPPORT if the storage family does not
             * match EndpointT.
             */
            template<typename EndpointT>
            /**
             * @brief Converts native storage to an endpoint after checking the address family.
             *
             * @return Endpoint on success, or WSAEAFNOSUPPORT if the storage family does not
             * match EndpointT.
             */
            static std::expected<EndpointT, ErrorT> fromStorage(const AddressStorageT& storage) noexcept{
                if constexpr(std::same_as<EndpointT, IPv4Endpoint>){
                    if(storage.ss_family != AF_INET)
                        return std::unexpected{WSAEAFNOSUPPORT};

                    sockaddr_in native{};
                    std::memcpy(&native, &storage, sizeof(native));
                    return IPv4Endpoint(
                        networkToHost32(native.sin_addr.s_addr),
                        networkToHost16(native.sin_port));
                }else if constexpr(std::same_as<EndpointT, IPv6Endpoint>){
                    if(storage.ss_family != AF_INET6)
                        return std::unexpected{WSAEAFNOSUPPORT};

                    sockaddr_in6 native{};
                    std::memcpy(&native, &storage, sizeof(native));

                    IPv6Endpoint::AddressBytes bytes{};
                    std::memcpy(bytes.data(), &native.sin6_addr.s6_addr, bytes.size());
                    return IPv6Endpoint(
                        bytes,
                        networkToHost16(native.sin6_port));
                }else{
                    static_assert(std::same_as<EndpointT, IPv4Endpoint>
                        || std::same_as<EndpointT, IPv6Endpoint>, "Unsupported endpoint type");
                }
            }

            /**
             * @brief Converts native storage to an endpoint under a trusted-family invariant.
             *
             * The address family is asserted in debug builds. Passing storage with a different
             * family is invalid use of the backend contract.
             */
            template<typename EndpointT>
            /**
             * @brief Converts native storage to an endpoint under a trusted-family invariant.
             *
             * The address family is asserted in debug builds. Passing storage with a different
             * family is invalid use of the backend contract.
             */
            static EndpointT fromStorageUnchecked(const AddressStorageT& storage) noexcept{
                if constexpr(std::same_as<EndpointT, IPv4Endpoint>){
                    assert(storage.ss_family == AF_INET);

                    sockaddr_in native{};
                    std::memcpy(&native, &storage, sizeof(native));
                    return IPv4Endpoint(
                        networkToHost32(native.sin_addr.s_addr),
                        networkToHost16(native.sin_port));
                }else if constexpr(std::same_as<EndpointT, IPv6Endpoint>){
                    assert(storage.ss_family == AF_INET6);

                    sockaddr_in6 native{};
                    std::memcpy(&native, &storage, sizeof(native));

                    IPv6Endpoint::AddressBytes bytes{};
                    std::memcpy(bytes.data(), &native.sin6_addr.s6_addr, bytes.size());
                    return IPv6Endpoint(
                        bytes,
                        networkToHost16(native.sin6_port));
                }else{
                    static_assert(std::same_as<EndpointT, IPv4Endpoint>
                        || std::same_as<EndpointT, IPv6Endpoint>, "Unsupported endpoint type");
                }
            }

            /**
             * @brief Sends bytes once through a native Winsock socket.
             *
             * @return Number of bytes sent, or WSAEMSGSIZE/native WSA error on failure.
             */
            static std::expected<std::size_t, ErrorT> send(const SocketT& s, std::span<const char> data) noexcept{
                if(data.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
                    return std::unexpected{WSAEMSGSIZE};

                const int sent = ::send(s, data.data(), static_cast<int>(data.size()), 0);
                if(sent == SOCKET_ERROR)
                    return std::unexpected{getLastError()};

                return static_cast<std::size_t>(sent);
            }

            /**
             * @brief Receives bytes once from a native Winsock socket.
             *
             * @return Number of bytes received, or WSAEMSGSIZE/native WSA error on failure.
             */
            static std::expected<std::size_t, ErrorT> receive(const SocketT& s, std::span<char> buffer) noexcept{
                if(buffer.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
                    return std::unexpected{WSAEMSGSIZE};

                const int received = ::recv(s, buffer.data(), static_cast<int>(buffer.size()), 0);
                if(received == SOCKET_ERROR)
                    return std::unexpected{getLastError()};

                return static_cast<std::size_t>(received);
            }

            /**
             * @brief Returns the current thread's last Winsock error.
             */
            static inline ErrorT getLastError() noexcept{ return static_cast<ErrorT>(WSAGetLastError()); }

            /**
             * @brief Formats a native Winsock error code as a string.
             */
            static std::string getMsgFromError(const ErrorT& err){
                std::string msg(512, '\0');

                //TODO: windows also supports wide chars but return
                //type has assumed to be narrow char for posix 
                //so maybe define generic? Support for wide chars is 
                //not really a concern either way
                auto written = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                    NULL,
                    static_cast<int>(err),
                    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                    msg.data(),
                    static_cast<DWORD>(msg.size()),
                    NULL
                );

                msg.resize(written);
                if(msg.empty())
                    return "Unknown Winsock error " + std::to_string(err);

                while(!msg.empty() && std::string_view{"\r\n "}.find(msg.back()) != std::string_view::npos)
                    msg.pop_back();

                return msg;
            }

            /**
             * @brief Returns the invalid socket sentinel.
             */
            static constexpr inline SocketT invalidSocket() noexcept{ return INVALID_SOCKET; };
    };
} // namespace ninttp::internal
