#include "socket.hpp"

#include <arpa/inet.h>

namespace ninttp{

    //for now most basic
    struct socket_error{};

    SocketBase::SocketBase(Domain domain, Service service, Protocol protocol) : domain_(domain),
                service_(service), protocol_(protocol), fdSocket_(-1){
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

    std::string sockaddrToString(const sockaddr* address) noexcept
    {
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

} //namesapce ninttp