#pragma once

#include "socket.hpp"
#include <cstring>
#include <arpa/inet.h>

namespace ninttp
{
    class TcpV4ClientSocket : public ClientSocket<v4Addr>{
        public:
            TcpV4ClientSocket()
                : ClientSocket<v4Addr>(Domain::IPv4, Service::Stream, Protocol::Tcp){}

            bool connect(const v4Addr& address, const Port& port) override {
                struct sockaddr_in sock;
                memset(&sock, 0, sizeof(sock));
                sock.sin_addr.s_addr = inet_addr(address.c_str());
                sock.sin_port = htons(port);
                sock.sin_family = toNative(domain_);

                return ::connect(fdSocket_, (struct sockaddr *)&sock,
                    static_cast<socklen_t>(sizeof(sock))) != -1;
            }
    };

    class TcpV6ClientSocket : public ClientSocket<v6Addr>{
        public:
            TcpV6ClientSocket()
                : ClientSocket<v6Addr>(Domain::IPv6, Service::Stream, Protocol::Tcp){}
            bool connect(const v6Addr& address, const Port& port) override {
                struct sockaddr_in6 sock;
                memset(&sock, 0, sizeof(sock));
                sock.sin6_addr = address;
                sock.sin6_port = htons(port);
                sock.sin6_family = toNative(domain_);

                return ::connect(fdSocket_, (struct sockaddr*)&sock,
                    static_cast<socklen_t>(sizeof(sock))) != -1;
            }
    };

    class UdpV4ClientSocket : public ClientSocket<v4Addr>{
        public:
            UdpV4ClientSocket()
                : ClientSocket<v4Addr>(Domain::IPv4, Service::Datagram, Protocol::Udp){}
            bool connect(const v4Addr& address, const Port& port) override {
                struct sockaddr_in sock;
                memset(&sock, 0, sizeof(sock));
                sock.sin_addr.s_addr = inet_addr(address.c_str());
                sock.sin_port = htons(port);
                sock.sin_family = toNative(domain_);

                return ::connect(fdSocket_, (struct sockaddr *)&sock,
                    static_cast<socklen_t>(sizeof(sock))) != -1;
            }
    };

    class UdpV6ClientSocket : public ClientSocket<v6Addr>{
        public:
            UdpV6ClientSocket()
                : ClientSocket<v6Addr>(Domain::IPv6, Service::Datagram, Protocol::Udp){}
            bool connect(const v6Addr& address, const Port& port) override {
                struct sockaddr_in6 sock;
                memset(&sock, 0, sizeof(sock));
                sock.sin6_addr = address;
                sock.sin6_port = htons(port);
                sock.sin6_family = toNative(domain_);

                return ::connect(fdSocket_, (struct sockaddr*)&sock,
                    static_cast<socklen_t>(sizeof(sock))) != -1;
            }
    };
}
