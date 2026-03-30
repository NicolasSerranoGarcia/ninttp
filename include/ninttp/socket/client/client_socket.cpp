#include "client_socket.hpp"

#include <cstring>
#include <arpa/inet.h>

namespace ninttp
{
    bool TcpV4ClientSocket::connect(const v4Addr& address, const Port& port){
        struct sockaddr_in sock;
        memset(&sock, 0, sizeof(sock));
        sock.sin_addr.s_addr = inet_addr(address.c_str());
        sock.sin_port = htons(port);
        sock.sin_family = toNative(domain_);

        return ::connect(fdSocket_, (struct sockaddr *)&sock,
            static_cast<socklen_t>(sizeof(sock))) != -1;
    }

    bool TcpV6ClientSocket::connect(const v6Addr& address, const Port& port){
        struct sockaddr_in6 sock;
        memset(&sock, 0, sizeof(sock));
        sock.sin6_addr = address;
        sock.sin6_port = htons(port);
        sock.sin6_family = toNative(domain_);

        return ::connect(fdSocket_, (struct sockaddr*)&sock,
            static_cast<socklen_t>(sizeof(sock))) != -1;
    }

    bool UdpV4ClientSocket::connect(const v4Addr& address, const Port& port){
        struct sockaddr_in sock;
        memset(&sock, 0, sizeof(sock));
        sock.sin_addr.s_addr = inet_addr(address.c_str());
        sock.sin_port = htons(port);
        sock.sin_family = toNative(domain_);

        return ::connect(fdSocket_, (struct sockaddr *)&sock,
            static_cast<socklen_t>(sizeof(sock))) != -1;
    }

    bool UdpV6ClientSocket::connect(const v6Addr& address, const Port& port){
        struct sockaddr_in6 sock;
        memset(&sock, 0, sizeof(sock));
        sock.sin6_addr = address;
        sock.sin6_port = htons(port);
        sock.sin6_family = toNative(domain_);

        return ::connect(fdSocket_, (struct sockaddr*)&sock,
            static_cast<socklen_t>(sizeof(sock))) != -1;
    }
}
