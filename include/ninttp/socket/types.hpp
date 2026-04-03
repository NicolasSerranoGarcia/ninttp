#pragma once

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
} // namespace ninttp