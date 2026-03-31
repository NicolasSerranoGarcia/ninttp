#pragma once

//TODO: organize on different folders (e.g server/, client/) etc...
//TODO: v6 version also support multicast, flow info..., so
//those versions might carry more atributes 
//TODO: if IPV6_V6ONLY is on, then v6 server sockets can still manage v4 requests
//in future manage that. For now, only v4 -> v4 and v6 -> v6
//TODO: what about collisions? if the user creates multiple 
//server listeners on the same interface

//TODO: add lots of c++ semantics and idioms

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>

#include <string>
#include <array>
#include <algorithm>
#include <unistd.h>
#include <cstring>

namespace ninttp
{

    enum class Domain{
        Invalid = -1,
        IPv4,
        IPv6,
        Unix,
    };

    enum class Service{
        Invalid = -1,
        Stream,
        Datagram,
        Raw,
        SeqPacket,
    };

    enum class Protocol{
        Invalid = -1,
        Default,
        Tcp,
        Udp,
    };

    constexpr int toNative(Domain domain) noexcept{
        switch (domain) {
            case Domain::IPv4: return AF_INET;
            case Domain::IPv6: return AF_INET6;
            case Domain::Unix: return AF_UNIX;
            default: return -1;
        }
    }

    constexpr int toNative(Service service) noexcept{
        switch (service) {
            case Service::Stream: return SOCK_STREAM;
            case Service::Datagram: return SOCK_DGRAM;
            case Service::Raw: return SOCK_RAW;
            case Service::SeqPacket: return SOCK_SEQPACKET;
            default: return -1;
        }
    }

    constexpr int toNative(Protocol protocol) noexcept{
        switch (protocol) {
            case Protocol::Default: return 0;
            case Protocol::Tcp: return IPPROTO_TCP;
            case Protocol::Udp: return IPPROTO_UDP;
            default: return -1;
        }
    }

    constexpr Domain toDomain(int nativeDomain) noexcept{
        switch (nativeDomain) {
            case AF_INET: return Domain::IPv4;
            case AF_INET6: return Domain::IPv6;
            case AF_UNIX: return Domain::Unix;
            default: return Domain::Invalid;
        }
    }

    constexpr Service toService(int nativeService) noexcept{
        switch (nativeService) {
            case SOCK_STREAM: return Service::Stream;
            case SOCK_DGRAM: return Service::Datagram;
            case SOCK_RAW: return Service::Raw;
            case SOCK_SEQPACKET: return Service::SeqPacket;
            default: return Service::Invalid;
        }
    }

    constexpr Protocol toProtocol(int nativeProtocol) noexcept{
        switch (nativeProtocol) {
            case 0: return Protocol::Default;
            case IPPROTO_TCP: return Protocol::Tcp;
            case IPPROTO_UDP: return Protocol::Udp;
            default: return Protocol::Invalid;
        }
    }

    //for now most basic
    struct socket_error{};

    inline std::string sockaddrToString(const sockaddr* address) noexcept{
        if (address == nullptr) {
            return {};
        }

        char buffer[INET6_ADDRSTRLEN]{};

        switch (address->sa_family) {
            case AF_INET: {
                const auto* ipv4 = reinterpret_cast<const sockaddr_in*>(address);
                if (::inet_ntop(AF_INET, &ipv4->sin_addr, buffer, sizeof(buffer)) == nullptr) {
                    return {};
                }
                return std::string{buffer};
            }

            case AF_INET6: {
                const auto* ipv6 = reinterpret_cast<const sockaddr_in6*>(address);
                if (::inet_ntop(AF_INET6, &ipv6->sin6_addr, buffer, sizeof(buffer)) == nullptr) {
                    return {};
                }
                return std::string{buffer};
            }

            case AF_UNIX: {
                const auto* local = reinterpret_cast<const sockaddr_un*>(address);
                return std::string{local->sun_path};
            }

            default:
                return {};
        }
    }

    //supports compressed IPv6 text through inet_pton
    struct v6Addr{
        std::array<uint16_t, 8> hextet{};

        constexpr bool empty() const noexcept
        {
            for (const uint16_t value : hextet) {
                if (value != 0) {
                    return false;
                }
            }

            return true;
        }

        constexpr v6Addr() = default;

        constexpr v6Addr(const std::array<uint16_t, 8>& value) noexcept
            : hextet(value){}

        v6Addr(const in6_addr& value) noexcept
        {
            for (std::size_t i = 0; i < hextet.size(); ++i) {
                const uint16_t native = static_cast<uint16_t>(
                    (static_cast<uint16_t>(value.s6_addr[i * 2]) << 8) |
                    static_cast<uint16_t>(value.s6_addr[i * 2 + 1])
                );

                hextet[i] = ntohs(native);
            }
        }

        v6Addr(const std::string& value) noexcept
        {
            in6_addr native{};
            if (::inet_pton(AF_INET6, value.c_str(), &native) == 1) {
                *this = v6Addr(native);
            }
        }

        v6Addr(const char* value) noexcept
        {
            if (value == nullptr) {
                return;
            }

            in6_addr native{};
            if (::inet_pton(AF_INET6, value, &native) == 1) {
                *this = v6Addr(native);
            }
        }

        operator std::array<uint16_t, 8>() const noexcept
        {
            return hextet;
        }

        operator in6_addr() const noexcept
        {
            in6_addr value{};

            for (std::size_t i = 0; i < hextet.size(); ++i) {
                const uint16_t network = htons(hextet[i]);
                value.s6_addr[i * 2] = static_cast<uint8_t>((network >> 8) & 0xFF);
                value.s6_addr[i * 2 + 1] = static_cast<uint8_t>(network & 0xFF);
            }

            return value;
        }
    };

    using v4Addr = std::string;

    inline constexpr const char* v4Addr_any = "0.0.0.0";
    inline constexpr const char* v4Addr_loopback = "172.0.0.1";
    inline constexpr v6Addr v6Addr_any{std::array<uint16_t, 8>{0, 0, 0, 0, 0, 0, 0, 0}};
    inline constexpr v6Addr v6Addr_loopback{std::array<uint16_t, 8>{0, 0, 0, 0, 0, 0, 0, 1}};

    //TODO: Port = whatever is used for Port on the platform
    //e.g on unix the type of the port passed to the functions, but windows may vary
    using Port = unsigned short;

    class IListener{
        public:
            virtual ~IListener() = default;
            virtual bool listen(const int n) = 0;

        protected:
            int backlog_ = 0;
    };

    //TODO: semantics of sockets? one time use like std::thread but can be reused as a var?
    //can close connection any time?

    //TODO: non visible? why should the user see this? only ClientSocket and ServerSocket
    class SocketBase{
        public:
            SocketBase() noexcept{};
            SocketBase(Domain domain, Service service, Protocol protocol)
                : domain_(domain), service_(service), protocol_(protocol), fdSocket_(-1){
                    const int nativeDomain = toNative(domain_);
                    const int nativeService = toNative(service_);
                    const int nativeProtocol = toNative(protocol_);

                    if (nativeDomain < 0 || nativeService < 0 || nativeProtocol < 0) {
                        throw socket_error{};
                    }

                    fdSocket_ = ::socket(nativeDomain, nativeService, nativeProtocol);

                    //TODO: maybe report from last error?
                    if(fdSocket_ == -1){
                        throw socket_error{};
                    }
                }
            SocketBase(int nativeDomain, int nativeService, int nativeProtocol)
            : SocketBase(toDomain(nativeDomain), toService(nativeService), toProtocol(nativeProtocol)){};

            virtual ~SocketBase(){
                if(!close()){
                    //check EBADF sockfd is not a valid file descriptor.
                    // EINVAL An invalid value was specified in how (but see BUGS).
                    // ENOTCONN The specified socket is not connected.
                    // ENOTSOCK The file descriptor sockfd does not refer to a socket.
                }
            };

        protected:
            Domain domain_;
            Service service_;
            Protocol protocol_;
            int fdSocket_;

        private:
            //TODO: make this public for reuse of the instance?
            //what about shutdown? 
            bool close(){
                //already closed
                if(fdSocket_ == -1){
                    return true;
                }

                auto closed = ::close(fdSocket_) == 0;

                if(closed){
                    fdSocket_ = -1;
                    return true;
                }

                return false;
            }
    };

    template <typename AddressT>
    class ServerSocket : public SocketBase{
        public:
            ServerSocket(Domain domain, Service service, Protocol protocol)
                : SocketBase(domain, service, protocol){}

            struct bindResult{
                
            };

            virtual bindResult bind(const AddressT& address, const Port& port) = 0;
    };

    template <typename AddressT>
    class ClientSocket : public SocketBase{
        public:
            ClientSocket(Domain domain, Service service, Protocol protocol)
                : SocketBase(domain, service, protocol){}
            //TODO: asume for now client only calls connect once, 
            //so no need t close a previous connection if there was
            //also return bool for now
            virtual bool connect(const AddressT& address, const Port& port) = 0;
    };

} // namespace ninttp
