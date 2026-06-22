#pragma once

#include "../../../endpoints.hpp"
#include "concepts.hpp"
#include "../../types.hpp"
#include "../../utils.hpp"

#include <expected>
#include <optional>
#include <span>

#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <cerrno>
#include <cstring> // strerror
#include <string>
#include <type_traits>
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

            static std::expected<SocketT, ErrorT> openSocket(Domain d, Service s, Protocol p) noexcept{
                const SocketT socket = static_cast<SocketT>(::socket(toNative(d), toNative(s), toNative(p)));
                if(socket == invalidSocket())
                    return std::unexpected{getLastError()};

                return socket;
            };

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
            
            static constexpr bool isUsableSocket(const SocketT& s) noexcept{ return s != invalidSocket(); };
            
            static std::expected<void, ErrorT> shutdownSocket(const SocketT& s, ShutdownPolicy what) noexcept{
                if(::shutdown(s, toNativeShutdownPolicy(what)) != 0)
                    return std::unexpected{getLastError()};

                return {};
            };

            static std::expected<void, ErrorT> bind(const SocketT& s, const AddressStorageT* addr, AddressLenT len) noexcept{
                if(::bind(s, reinterpret_cast<const sockaddr*>(addr), len) != 0)
                    return std::unexpected{getLastError()};

                return {};
            }

            static std::expected<void, ErrorT> listen(const SocketT& s, int backlog) noexcept{
                if(::listen(s, backlog) != 0)
                    return std::unexpected{getLastError()};

                return {};
            }

            static std::expected<AddressBundleT, ErrorT> accept(const SocketT& s) noexcept{
                AddressStorageT storage{};
                AddressLenT len = sizeof(storage);

                SocketT sock = ::accept(s, reinterpret_cast<sockaddr*>(&storage), &len);

                if(sock == invalidSocket())
                    return std::unexpected{getLastError()};

                return AddressBundleT{sock, storage, len};
            }

            static std::expected<void, ErrorT> connect(const SocketT& s, const AddressStorageT* addr, AddressLenT len) noexcept{
                if(::connect(s, reinterpret_cast<const sockaddr*>(addr), len) != 0)
                    return std::unexpected{getLastError()};

                return {};
            }

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

            template<typename EndpointT>
            static std::expected<EndpointT, ErrorT> fromStorage(const AddressStorageT& storage) noexcept{
                if constexpr(std::same_as<EndpointT, ninttp::IPv4Endpoint>){
                    if(storage.ss_family != AF_INET)
                        return std::unexpected{EAFNOSUPPORT};

                    sockaddr_in native{};
                    std::memcpy(&native, &storage, sizeof(native));
                    return ninttp::IPv4Endpoint(
                        ninttp::networkToHost32(native.sin_addr.s_addr),
                        ninttp::networkToHost16(native.sin_port));
                }else{
                    static_assert(std::same_as<EndpointT, ninttp::IPv4Endpoint>, "Unsupported endpoint type");
                }
            }

            static std::expected<std::size_t, ErrorT> send(const SocketT& s, std::span<const char> data) noexcept{
                const auto sent = ::send(s, data.data(), data.size(), 0);
                if(sent == -1)
                    return std::unexpected{getLastError()};

                return static_cast<std::size_t>(sent);
            }

            static std::expected<std::size_t, ErrorT> receive(const SocketT& s, std::span<char> buffer) noexcept{
                const auto received = ::recv(s, buffer.data(), buffer.size(), 0);
                if(received == -1)
                    return std::unexpected{getLastError()};

                return static_cast<std::size_t>(received);
            }

            //native error code
            static inline ErrorT getLastError() noexcept{ return static_cast<ErrorT>(errno); };

            static std::string getMsgFromError(const ErrorT& err){
                return std::string(strerror(err));
            }

            //this is the only reliable way I found to be explicit about an invalidSocket when we assume they may not be copyable. Creating
            //an instance guarantees no copy
            static constexpr inline SocketT invalidSocket() noexcept{ return static_cast<SocketT>(-1); }
    };

} // namespace ninttp::internal
