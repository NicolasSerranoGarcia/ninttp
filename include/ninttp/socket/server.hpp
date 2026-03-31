#pragma once

//TODO: again, lifetime of the objects, what to do?

#include "socket.hpp"
#include <cstring>
#include <arpa/inet.h>

namespace ninttp
{
    //only difference between this and clientSocket
    //is semantic. This puts emphasis in that it was created in the server
    //side, and that it comes from the Listener
    //Also, should construct
    class TcpV4StreamSocket : public SocketBase{
        public:
            TcpV4StreamSocket() : SocketBase(Domain::IPv4, Service::Stream, Protocol::Tcp){};

            Port getPeerPort() const noexcept{ return peerPort_; };

            v4Addr getPeerAddress() const noexcept{ return peerAddr_; };

        private:
            v4Addr peerAddr_;
            Port peerPort_;

        friend class TcpV4ListenerSocket;
    };

    class TcpV6StreamSocket : public SocketBase{
        public:
            TcpV6StreamSocket() : SocketBase(Domain::IPv6, Service::Stream, Protocol::Tcp){};

            Port getPeerPort() const noexcept{ return peerPort_; };

            v6Addr getPeerAddress() const noexcept{ return peerAddr_; };
        private:
            v6Addr peerAddr_;
            Port peerPort_;
        friend class TcpV6ListenerSocket;
    };

    class TcpV4ListenerSocket : public ServerSocket<v4Addr>, public IListener{
        public:
            TcpV4ListenerSocket()
                : ServerSocket<v4Addr>(Domain::IPv4, Service::Stream, Protocol::Tcp){}

            //should be called once
            bindResult bind(const v4Addr& address, const Port& port) override {
                struct sockaddr_in sock;
                memset(&sock, 0, sizeof(sock));
                sock.sin_addr.s_addr = inet_addr(address.c_str());
                sock.sin_port = htons(port);
                sock.sin_family = toNative(domain_);

                ::bind(fdSocket_, (struct sockaddr*)&sock,
                    static_cast<socklen_t>(sizeof(sock)));

                return {};
            }

            //should be called once
            bool listen(const int n) override {
                backlog_ = n;
                return ::listen(fdSocket_, backlog_) != -1;
            }
            TcpV4StreamSocket acceptOne() {
                TcpV4StreamSocket ss;
                struct sockaddr_in cli;
                socklen_t cli_len;
                ss.fdSocket_ = accept(fdSocket_, (struct sockaddr*)&cli, &cli_len);
                if(ss.fdSocket_ == -1){
                    //TODO: manage
                    throw socket_error{};
                }
                ss.peerAddr_ = sockaddrToString((struct sockaddr*)&cli);
                ss.peerPort_ = cli.sin_port;
                return ss;
            }
    };

    class TcpV6ListenerSocket : public ServerSocket<v6Addr>, public IListener{
        public:
            TcpV6ListenerSocket()
                : ServerSocket<v6Addr>(Domain::IPv6, Service::Stream, Protocol::Tcp){}
            bindResult bind(const v6Addr& address, const Port& port) override {
                struct sockaddr_in6 sock;
                memset(&sock, 0, sizeof(sock));
                sock.sin6_addr = address;
                sock.sin6_port = htons(port);
                sock.sin6_family = toNative(domain_);

                ::bind(fdSocket_, (struct sockaddr*)&sock,
                    static_cast<socklen_t>(sizeof(sock)));

                return {};
            }
            bool listen(const int n) override {
                backlog_ = n;
                return ::listen(fdSocket_, backlog_) != -1;
            }
            TcpV6StreamSocket acceptOne() {
                TcpV6StreamSocket ss;
                struct sockaddr_in6 cli;
                socklen_t cli_len;
                ss.fdSocket_ = accept(fdSocket_, (struct sockaddr*)&cli, &cli_len);
                if(ss.fdSocket_ == -1){
                    //TODO: manage
                    throw socket_error{};
                }
                ss.peerAddr_ = sockaddrToString((struct sockaddr*)&cli);
                ss.peerPort_ = cli.sin6_port;
                //TODO: maybe the connection carries more info for ipv6
                return ss;
            }
    };

    class UdpV4ServerSocket : public ServerSocket<v4Addr>{
        public:
            UdpV4ServerSocket()
                : ServerSocket<v4Addr>(Domain::IPv4, Service::Datagram, Protocol::Udp){}
            bindResult bind(const v4Addr& address, const Port& port) override {
                struct sockaddr_in sock;
                memset(&sock, 0, sizeof(sock));
                sock.sin_addr.s_addr = inet_addr(address.c_str());
                sock.sin_port = htons(port);
                sock.sin_family = toNative(domain_);

                ::bind(fdSocket_, (struct sockaddr*)&sock,
                    static_cast<socklen_t>(sizeof(sock)));

                return {};
            }
    };

    class UdpV6ServerSocket : public ServerSocket<v6Addr>{
        public:
            UdpV6ServerSocket()
                : ServerSocket<v6Addr>(Domain::IPv6, Service::Datagram, Protocol::Udp){}
            bindResult bind(const v6Addr& address, const Port& port) override {
                struct sockaddr_in6 sock;
                memset(&sock, 0, sizeof(sock));
                sock.sin6_addr = address;
                sock.sin6_port = htons(port);
                sock.sin6_family = toNative(domain_);

                ::bind(fdSocket_, (struct sockaddr*)&sock,
                    static_cast<socklen_t>(sizeof(sock)));

                return {};
            }
    };
}
