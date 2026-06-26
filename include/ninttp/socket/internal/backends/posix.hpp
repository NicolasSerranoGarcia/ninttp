/**
 * @file posix.hpp
 * @author Nicolas Serrano (serranogarcianicolas@gmail.com)
 * @brief Defines the POSIX socket backend implementation.
 * @version 0.1
 * @date 2026-06-26
 *
 * @copyright Copyright (c) 2026 Nicolas Serrano Garcia
 *
 */

#pragma once

#include "../../../endpoints.hpp"
#include "concepts.hpp"
#include "../../types.hpp"
#include "../../utils.hpp"

#include <concepts>
#include <expected>
#include <optional>
#include <span>

#include <array>
#include <cassert>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <limits>
#include <cstring> // strerror_r
#include <string>
#include <unistd.h>


namespace ninttp::internal
{

    static constexpr inline int toNative(Domain domain) noexcept{
        switch (domain) {
            case Domain::IPv4: return AF_INET;
            case Domain::IPv6: return AF_INET6;
            case Domain::Unix: return AF_UNIX;
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
    
    static constexpr inline Domain toDomain(int nativeDomain) noexcept{
        switch (nativeDomain) {
            case AF_INET: return Domain::IPv4;
            case AF_INET6: return Domain::IPv6;
            case AF_UNIX: return Domain::Unix;
            default: return Domain::Invalid;
        }
    }

    static constexpr inline Service toService(int nativeService) noexcept{
        switch (nativeService) {
            case SOCK_STREAM: return Service::Stream;
            case SOCK_DGRAM: return Service::Datagram;
            case SOCK_RAW: return Service::Raw;
            case SOCK_SEQPACKET: return Service::SeqPacket;
            default: return Service::Invalid;
        }
    }
    
    static constexpr inline Protocol toProtocol(int nativeProtocol) noexcept{
        switch (nativeProtocol) {
            case 0: return Protocol::Default;
            case IPPROTO_TCP: return Protocol::Tcp;
            case IPPROTO_UDP: return Protocol::Udp;
            default: return Protocol::Invalid;
        }
    }
    
    static constexpr inline int toNativeShutdownPolicy(ShutdownPolicy what) noexcept{
        switch(what){
            case ShutdownPolicy::SHUT_RECEPTIONS:
                return SHUT_RD;
            case ShutdownPolicy::SHUT_TRANSMISSIONS:
                return SHUT_WR;
            case ShutdownPolicy::SHUT_TRANSMISSIONS_AND_RECEPTIONS:
                return SHUT_RDWR;
            default:
                return -1;
        }
    }

    /**
     * @brief POSIX implementation of the SocketBackend contract.
     */
    class PosixBackend{
        public:
            using SocketT = int;
            using ErrorT = int;
            using AddressStorageT = sockaddr_storage;
            using AddressLenT = socklen_t;
            using CloseStatusT = SocketCloseStatus<ErrorT>;

            struct AddressBundleT{
                SocketT socket;
                AddressStorageT storage;
                AddressLenT len;
            };

            /**
             * @brief Enables close-on-exec for a native descriptor when configured.
             *
             * @param s Native POSIX socket descriptor.
             * @return Empty result on success, or errno captured from fcntl().
             */
            static std::expected<void, ErrorT> setCloseOnExec(const SocketT& s) noexcept{
                #if NINTTP_FD_CLOSE_EXEC == 1
                const int flags = ::fcntl(s, F_GETFD);
                if(flags == -1)
                    return std::unexpected{getLastError()};

                if(::fcntl(s, F_SETFD, flags | FD_CLOEXEC) == -1)
                    return std::unexpected{getLastError()};
                #endif

                return {};
            }

            /**
             * @brief Opens a native POSIX socket and applies required backend invariants.
             *
             * On BSD/Apple, SIGPIPE suppression is part of the backend invariant. If applying
             * that option fails after the socket has been opened, this function closes the socket
             * before returning the option error. That cleanup close is best effort: in the worst
             * ambiguous-close case, the socket may leak because there is no place in this return
             * type to report both the option failure and an uncertain cleanup result.
             */
            static std::expected<SocketT, ErrorT> openSocket(Domain d, Service s, Protocol p) noexcept{
                int nativeService = toNative(s);
                #if NINTTP_FD_CLOSE_EXEC == 1 && defined(SOCK_CLOEXEC)
                nativeService |= SOCK_CLOEXEC;
                #endif

                const SocketT socket = static_cast<SocketT>(::socket(toNative(d), nativeService, toNative(p)));
                if(socket == invalidSocket())
                    return std::unexpected{getLastError()};

                #if NINTTP_FD_CLOSE_EXEC == 1 && !defined(SOCK_CLOEXEC)
                if(auto closeOnExec = setCloseOnExec(socket); !closeOnExec.has_value()){
                    const ErrorT error = closeOnExec.error();
                    ::close(socket);
                    return std::unexpected{error};
                }
                #endif

                #if NINTTP_PLATFORM_BSD == 1 || NINTTP_PLATFORM_APPLE == 1
                const int noSigPipe = 1;
                if(::setsockopt(socket, SOL_SOCKET, SO_NOSIGPIPE, &noSigPipe, sizeof(noSigPipe)) != 0){
                    const ErrorT error = getLastError();
                    ::close(socket);
                    return std::unexpected{error};
                }
                #endif

                return socket;
            };

            /**
             * @brief Closes a native POSIX socket descriptor.
             *
             * @param s Native POSIX socket descriptor.
             * @return Ownership disposition and optional errno captured from close().
             */
            static CloseStatusT closeSocket(const SocketT& s) noexcept{
                if(::close(s) == 0)
                    return {SocketCloseDisposition::Released, std::nullopt};

                const ErrorT error = getLastError();

                // Linux, Android, and the BSDs release a valid descriptor even
                // when close reports an error. Apple documents the errors but
                // not the descriptor disposition, so keep that result honest.
                #if NINTTP_PLATFORM_LINUX == 1 || NINTTP_PLATFORM_ANDROID == 1 || NINTTP_PLATFORM_BSD == 1
                    return {SocketCloseDisposition::Released, error};
                #else
                    if(error == EBADF)
                        return {SocketCloseDisposition::Released, error};

                    return {SocketCloseDisposition::Unspecified, error};
                #endif
            };
            
            /**
             * @brief Checks whether a descriptor is not the backend invalid sentinel.
             */
            static constexpr bool isUsableSocket(const SocketT& s) noexcept{ return s != invalidSocket(); };
            
            /**
             * @brief Applies native shutdown() to the socket.
             *
             * @return Empty result on success, or errno captured from shutdown().
             */
            static std::expected<void, ErrorT> shutdownSocket(const SocketT& s, ShutdownPolicy what) noexcept{
                if(::shutdown(s, toNativeShutdownPolicy(what)) != 0)
                    return std::unexpected{getLastError()};

                return {};
            };

            /**
             * @brief Enables a supported socket option.
             *
             * @return Empty result on success, EOPNOTSUPP for unknown options, or errno from
             * setsockopt().
             */
            static std::expected<void, ErrorT> setOption(const SocketT& s, SocketOption option) noexcept{
                switch(option){
                    case SocketOption::IPv6Only: {
                        const int nativeValue = 1;
                        if(::setsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY, &nativeValue, sizeof(nativeValue)) != 0)
                            return std::unexpected{getLastError()};

                        return {};
                    }
                }

                return std::unexpected{EOPNOTSUPP};
            }

            /**
             * @brief Disables a supported socket option.
             *
             * @return Empty result on success, EOPNOTSUPP for unknown options, or errno from
             * setsockopt().
             */
            static std::expected<void, ErrorT> unsetOption(const SocketT& s, SocketOption option) noexcept{
                switch(option){
                    case SocketOption::IPv6Only: {
                        const int nativeValue = 0;
                        if(::setsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY, &nativeValue, sizeof(nativeValue)) != 0)
                            return std::unexpected{getLastError()};

                        return {};
                    }
                }

                return std::unexpected{EOPNOTSUPP};
            }

            /**
             * @brief Binds a native socket descriptor to a native address.
             *
             * @return Empty result on success, or errno captured from bind().
             */
            static std::expected<void, ErrorT> bind(const SocketT& s, const AddressStorageT* addr, AddressLenT len) noexcept{
                if(::bind(s, reinterpret_cast<const sockaddr*>(addr), len) != 0)
                    return std::unexpected{getLastError()};

                return {};
            }

            /**
             * @brief Marks a bound native socket as a listener.
             *
             * @return Empty result on success, or errno captured from listen().
             */
            static std::expected<void, ErrorT> listen(const SocketT& s, int backlog) noexcept{
                if(::listen(s, backlog) != 0)
                    return std::unexpected{getLastError()};

                return {};
            }

            /**
             * @brief Accepts a native POSIX connection and applies required backend invariants.
             *
             * On BSD/Apple, SIGPIPE suppression is part of the backend invariant for accepted
             * sockets. If applying that option fails after accept() has returned a socket, this
             * function closes the accepted socket before returning the option error. That cleanup
             * close is best effort: in the worst ambiguous-close case, the accepted socket may
             * leak because there is no place in this return type to report both the option failure
             * and an uncertain cleanup result.
             */
            static std::expected<AddressBundleT, ErrorT> accept(const SocketT& s) noexcept{
                AddressStorageT storage{};
                AddressLenT len = sizeof(storage);

                #if NINTTP_FD_CLOSE_EXEC == 1 && NINTTP_HAVE_ACCEPT4 == 1 && defined(SOCK_CLOEXEC)
                SocketT sock = ::accept4(s, reinterpret_cast<sockaddr*>(&storage), &len, SOCK_CLOEXEC);
                #else
                SocketT sock = ::accept(s, reinterpret_cast<sockaddr*>(&storage), &len);
                #endif

                if(sock == invalidSocket())
                    return std::unexpected{getLastError()};

                #if NINTTP_FD_CLOSE_EXEC == 1 && !(NINTTP_HAVE_ACCEPT4 == 1 && defined(SOCK_CLOEXEC))
                if(auto closeOnExec = setCloseOnExec(sock); !closeOnExec.has_value()){
                    const ErrorT error = closeOnExec.error();
                    ::close(sock);
                    return std::unexpected{error};
                }
                #endif

                #if NINTTP_PLATFORM_BSD == 1 || NINTTP_PLATFORM_APPLE == 1
                const int noSigPipe = 1;
                if(::setsockopt(sock, SOL_SOCKET, SO_NOSIGPIPE, &noSigPipe, sizeof(noSigPipe)) != 0){
                    const ErrorT error = getLastError();
                    ::close(sock);
                    return std::unexpected{error};
                }
                #endif

                return AddressBundleT{sock, storage, len};
            }

            /**
             * @brief Connects a native socket descriptor to a native address.
             *
             * @return Empty result on success, or errno captured from connect().
             */
            static std::expected<void, ErrorT> connect(const SocketT& s, const AddressStorageT* addr, AddressLenT len) noexcept{
                if(::connect(s, reinterpret_cast<const sockaddr*>(addr), len) != 0)
                    return std::unexpected{getLastError()};

                return {};
            }

            /**
             * @brief Converts an IPv4 endpoint to POSIX address storage.
             */
            static AddressStorageT toStorage(const ninttp::IPv4Endpoint& endpoint) noexcept{
                sockaddr_in native{};
                native.sin_family = AF_INET;
                native.sin_addr.s_addr = ninttp::hostToNetwork32(endpoint.addressHostOrder());
                native.sin_port = ninttp::hostToNetwork16(endpoint.portHostOrder());

                AddressStorageT storage{};
                std::memcpy(&storage, &native, sizeof(native));
                return storage;
            }

            static constexpr AddressLenT storageLen(const ninttp::IPv4Endpoint&) noexcept{
                return static_cast<AddressLenT>(sizeof(sockaddr_in));
            }

            /**
             * @brief Converts an IPv6 endpoint to POSIX address storage.
             */
            static AddressStorageT toStorage(const ninttp::IPv6Endpoint& endpoint) noexcept{
                sockaddr_in6 native{};
                native.sin6_family = AF_INET6;
                native.sin6_port = ninttp::hostToNetwork16(endpoint.portHostOrder());

                const auto bytes = endpoint.addressBytes();
                std::memcpy(&native.sin6_addr.s6_addr, bytes.data(), bytes.size());

                AddressStorageT storage{};
                std::memcpy(&storage, &native, sizeof(native));
                return storage;
            }

            static constexpr AddressLenT storageLen(const ninttp::IPv6Endpoint&) noexcept{
                return static_cast<AddressLenT>(sizeof(sockaddr_in6));
            }

            /**
             * @brief Converts native storage to an endpoint after checking the address family.
             *
             * @return Endpoint on success, or EAFNOSUPPORT if the storage family does not match
             * EndpointT.
             */
            template<typename EndpointT>
            /**
             * @brief Converts native storage to an endpoint after checking the address family.
             *
             * @return Endpoint on success, or EAFNOSUPPORT if the storage family does not match
             * EndpointT.
             */
            static std::expected<EndpointT, ErrorT> fromStorage(const AddressStorageT& storage) noexcept{
                if constexpr(std::same_as<EndpointT, ninttp::IPv4Endpoint>){
                    if(storage.ss_family != AF_INET)
                        return std::unexpected{EAFNOSUPPORT};

                    sockaddr_in native{};
                    std::memcpy(&native, &storage, sizeof(native));
                    return ninttp::IPv4Endpoint(
                        ninttp::networkToHost32(native.sin_addr.s_addr),
                        ninttp::networkToHost16(native.sin_port));
                }else if constexpr(std::same_as<EndpointT, ninttp::IPv6Endpoint>){
                    if(storage.ss_family != AF_INET6)
                        return std::unexpected{EAFNOSUPPORT};

                    sockaddr_in6 native{};
                    std::memcpy(&native, &storage, sizeof(native));

                    ninttp::IPv6Endpoint::AddressBytes bytes{};
                    std::memcpy(bytes.data(), &native.sin6_addr.s6_addr, bytes.size());
                    return ninttp::IPv6Endpoint(
                        bytes,
                        ninttp::networkToHost16(native.sin6_port));
                }else{
                    static_assert(std::same_as<EndpointT, ninttp::IPv4Endpoint>
                        || std::same_as<EndpointT, ninttp::IPv6Endpoint>, "Unsupported endpoint type");
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
                if constexpr(std::same_as<EndpointT, ninttp::IPv4Endpoint>){
                    assert(storage.ss_family == AF_INET);

                    sockaddr_in native{};
                    std::memcpy(&native, &storage, sizeof(native));
                    return ninttp::IPv4Endpoint(
                        ninttp::networkToHost32(native.sin_addr.s_addr),
                        ninttp::networkToHost16(native.sin_port));
                }else if constexpr(std::same_as<EndpointT, ninttp::IPv6Endpoint>){
                    assert(storage.ss_family == AF_INET6);

                    sockaddr_in6 native{};
                    std::memcpy(&native, &storage, sizeof(native));

                    ninttp::IPv6Endpoint::AddressBytes bytes{};
                    std::memcpy(bytes.data(), &native.sin6_addr.s6_addr, bytes.size());
                    return ninttp::IPv6Endpoint(
                        bytes,
                        ninttp::networkToHost16(native.sin6_port));
                }else{
                    static_assert(std::same_as<EndpointT, ninttp::IPv4Endpoint>
                        || std::same_as<EndpointT, ninttp::IPv6Endpoint>, "Unsupported endpoint type");
                }
            }

            /**
             * @brief Sends bytes once through a native POSIX socket.
             *
             * @return Number of bytes sent, or EMSGSIZE/native errno on failure.
             */
            static std::expected<std::size_t, ErrorT> send(const SocketT& s, std::span<const char> data) noexcept{
                if(data.size() > static_cast<std::size_t>(std::numeric_limits<ssize_t>::max()))
                    return std::unexpected{EMSGSIZE};

                //on linux and android (linux based), the signal SIGPIPE is raised if the peer closed the connection and we try sending
                //bytes. We avoid handling it by passing the MSG_NOSIGNAL option
                #if NINTTP_PLATFORM_LINUX == 1 || NINTTP_PLATFORM_ANDROID == 1
                const auto sent = ::send(s, data.data(), data.size(), MSG_NOSIGNAL);
                #else
                const auto sent = ::send(s, data.data(), data.size(), 0);
                #endif
                if(sent == -1)
                    return std::unexpected{getLastError()};

                return static_cast<std::size_t>(sent);
            }

            /**
             * @brief Receives bytes once from a native POSIX socket.
             *
             * @return Number of bytes received, or errno captured from recv().
             */
            static std::expected<std::size_t, ErrorT> receive(const SocketT& s, std::span<char> buffer) noexcept{
                const auto received = ::recv(s, buffer.data(), buffer.size(), 0);
                if(received == -1)
                    return std::unexpected{getLastError()};

                return static_cast<std::size_t>(received);
            }

            /**
             * @brief Returns the current thread's errno value.
             */
            static inline ErrorT getLastError() noexcept{ return static_cast<ErrorT>(errno); };

            /**
             * @brief Formats a native POSIX error code as a string.
             */
            static std::string getMsgFromError(const ErrorT& err){
                std::array<char, 256> buffer{};
                return msgFromStrerrorResult(
                    err,
                    buffer,
                    ::strerror_r(err, buffer.data(), buffer.size()));
            }

            /**
             * @brief Returns the invalid descriptor sentinel.
             */
            static constexpr inline SocketT invalidSocket() noexcept{ return static_cast<SocketT>(-1); }

        private:
            static std::string msgFromStrerrorResult(
                const ErrorT&,
                const std::array<char, 256>&,
                char* result){
                return std::string(result);
            }

            static std::string msgFromStrerrorResult(
                const ErrorT& err,
                const std::array<char, 256>& buffer,
                int result){
                if(result == 0)
                    return std::string(buffer.data());

                return "Unknown POSIX error " + std::to_string(err);
            }
    };

} // namespace ninttp::internal
