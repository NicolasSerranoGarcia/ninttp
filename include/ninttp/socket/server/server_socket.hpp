#pragma once

//TODO: again, lifetime of the objects, what to do?

#include "../socket.hpp"

namespace ninttp
{
    //only difference between this and clientSocket
    //is semantic. This puts emphasis in that it was created in the server
    //side, and that it comes from the Listener
    //Also, should construct
    class TcpV4StreamSocket : public SocketBase{
        public:
            TcpV4StreamSocket() : SocketBase(Domain::IPv4, Service::Stream, Protocol::Tcp){};

        private:
            v4Addr peer_;
            Port peerPort_;
        friend class TcpV4ListenerSocket;
    };

    class TcpV6StreamSocket : public SocketBase{
        public:
            TcpV6StreamSocket() : SocketBase(Domain::IPv6, Service::Stream, Protocol::Tcp){};

        private:
            v6Addr peer_;
            Port peerPort_;
        friend class TcpV6ListenerSocket;
    };

    class TcpV4ListenerSocket : public ServerSocket<v4Addr>, public IListener{
        public:
            TcpV4ListenerSocket()
                : ServerSocket<v4Addr>(Domain::IPv4, Service::Stream, Protocol::Tcp){}
            //should be called once
            bool bind(const v4Addr& address, const Port& port) override;
            //should be called once
            bool listen(const int n) override;
            TcpV4StreamSocket acceptOne();
    };

    class TcpV6ListenerSocket : public ServerSocket<v6Addr>, public IListener{
        public:
            TcpV6ListenerSocket()
                : ServerSocket<v6Addr>(Domain::IPv6, Service::Stream, Protocol::Tcp){}
            bool bind(const v6Addr& address, const Port& port) override;
            bool listen(const int n) override;
            TcpV6StreamSocket acceptOne();
    };

    class UdpV4ServerSocket : public ServerSocket<v4Addr>{
        public:
            UdpV4ServerSocket()
                : ServerSocket<v4Addr>(Domain::IPv4, Service::Datagram, Protocol::Udp){}
            bool bind(const v4Addr& address, const Port& port) override;
    };

    class UdpV6ServerSocket : public ServerSocket<v6Addr>{
        public:
            UdpV6ServerSocket()
                : ServerSocket<v6Addr>(Domain::IPv6, Service::Datagram, Protocol::Udp){}
            bool bind(const v6Addr& address, const Port& port) override;
    };
}
