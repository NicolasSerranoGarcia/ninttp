#pragma once

#include "../../types.hpp"

#include <optional>

#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <cerrno>
#include <cstring> // strerror
#include <string>
#include <unistd.h>


namespace ninttp::internal
{

    static constexpr int toNative(Domain domain) noexcept{
        switch (domain) {
            case Domain::IPv4: return AF_INET;
            case Domain::IPv6: return AF_INET6;
            case Domain::Unix: return AF_UNIX;
            default: return -1;
        }
    }
    
    static constexpr int toNative(Service service) noexcept{
        switch (service) {
            case Service::Stream: return SOCK_STREAM;
            case Service::Datagram: return SOCK_DGRAM;
            case Service::Raw: return SOCK_RAW;
            case Service::SeqPacket: return SOCK_SEQPACKET;
            default: return -1;
        }
    }
    
    static constexpr int toNative(Protocol protocol) noexcept{
        switch (protocol) {
            case Protocol::Default: return 0;
            case Protocol::Tcp: return IPPROTO_TCP;
            case Protocol::Udp: return IPPROTO_UDP;
            default: return -1;
        }
    }
    
    static constexpr Domain toDomain(int nativeDomain) noexcept{
        switch (nativeDomain) {
            case AF_INET: return Domain::IPv4;
            case AF_INET6: return Domain::IPv6;
            case AF_UNIX: return Domain::Unix;
            default: return Domain::Invalid;
        }
    }

    static constexpr Service toService(int nativeService) noexcept{
        switch (nativeService) {
            case SOCK_STREAM: return Service::Stream;
            case SOCK_DGRAM: return Service::Datagram;
            case SOCK_RAW: return Service::Raw;
            case SOCK_SEQPACKET: return Service::SeqPacket;
            default: return Service::Invalid;
        }
    }
    
    static constexpr Protocol toProtocol(int nativeProtocol) noexcept{
        switch (nativeProtocol) {
            case 0: return Protocol::Default;
            case IPPROTO_TCP: return Protocol::Tcp;
            case IPPROTO_UDP: return Protocol::Udp;
            default: return Protocol::Invalid;
        }
    }
    
    static constexpr int toNativeShutdownPolicy(ShutdownPolicy what) noexcept{
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

            struct AddressBundleT{
                SocketT socket;
                AddressStorageT storage;
                AddressLenT len;
            };

            //returns a valid SocketT or INVALID_SOCKET on error. Check getLastError
            static SocketT openSocket(Domain d, Service s, Protocol p) noexcept{
                return static_cast<SocketT>(::socket(toNative(d), toNative(s), toNative(p)));
            };

            static bool closeSocket(const SocketT& s) noexcept{ return ::close(s) == 0; };
            
            static constexpr bool isValidSocket(const SocketT& s) noexcept{ return s != INVALID_SOCKET; };
            
            static bool shutdownSocket(const SocketT& s, ShutdownPolicy what) noexcept{
                return ::shutdown(s, toNativeShutdownPolicy(what)) == 0;
            };

            static bool bind(const SocketT& s, const AddressStorageT* addr, AddressLenT len) noexcept{
                return ::bind(s, reinterpret_cast<const sockaddr*>(addr), len) == 0;
            }

            static bool listen(const SocketT& s, int backlog) noexcept{
                return ::listen(s, backlog) == 0;
            }

            static std::optional<AddressBundleT> accept(const SocketT& s) noexcept{
                AddressStorageT storage;
                AddressLenT len = sizeof(storage);

                SocketT sock = ::accept(s, reinterpret_cast<sockaddr*>(&storage), &len);

                if(sock == -1)
                    return {};

                return AddressBundleT{sock, storage, len};
            }

            static bool connect(const SocketT& s, const AddressStorageT* addr, AddressLenT len) noexcept{
                return ::connect(s, reinterpret_cast<const sockaddr*>(addr), len) == 0;
            }

            static ssize_t send(const SocketT& s, const char* buff, size_t n) noexcept{
                return ::send(s, buff, n, 0);
            }

            static ssize_t receive(const SocketT& s, char* buff, size_t n) noexcept{
                return ::recv(s, buff, n, 0);
            }


            //native error code
            static ErrorT getLastError() noexcept{ return static_cast<int>(errno); };

            static std::string getMsgFromError(const ErrorT& err){
                return std::string(strerror(err));
            }

            static constexpr SocketT INVALID_SOCKET = -1;

            static constexpr SocketT invalidSocket() noexcept{ return INVALID_SOCKET; }
    };

} // namespace ninttp::internal
