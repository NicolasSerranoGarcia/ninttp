#pragma once

#include "../socket.hpp"

namespace ninttp
{
    class TcpV4ClientSocket : public ClientSocket<v4Addr>{
        public:
            TcpV4ClientSocket()
                : ClientSocket<v4Addr>(Domain::IPv4, Service::Stream, Protocol::Tcp){}

            bool connect(const v4Addr& address, const Port& port) override;
    };

    class TcpV6ClientSocket : public ClientSocket<v6Addr>{
        public:
            TcpV6ClientSocket()
                : ClientSocket<v6Addr>(Domain::IPv6, Service::Stream, Protocol::Tcp){}
            bool connect(const v6Addr& address, const Port& port) override;
    };

    class UdpV4ClientSocket : public ClientSocket<v4Addr>{
        public:
            UdpV4ClientSocket()
                : ClientSocket<v4Addr>(Domain::IPv4, Service::Datagram, Protocol::Udp){}
            bool connect(const v4Addr& address, const Port& port) override;
    };

    class UdpV6ClientSocket : public ClientSocket<v6Addr>{
        public:
            UdpV6ClientSocket()
                : ClientSocket<v6Addr>(Domain::IPv6, Service::Datagram, Protocol::Udp){}
            bool connect(const v6Addr& address, const Port& port) override;
    };
}
