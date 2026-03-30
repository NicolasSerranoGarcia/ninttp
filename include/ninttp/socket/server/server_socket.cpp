#include "server_socket.hpp"

//TODO: make further reorganization of files 

#include <cstring>
#include <arpa/inet.h>

namespace ninttp
{
    struct socket_error{};

    bool TcpV4ListenerSocket::bind(const v4Addr& address, const Port& port){
        struct sockaddr_in sock;
        memset(&sock, 0, sizeof(sock));
        sock.sin_addr.s_addr = inet_addr(address.c_str());
        sock.sin_port = htons(port);
        sock.sin_family = toNative(domain_);

        return ::bind(fdSocket_, (struct sockaddr*)&sock,
            static_cast<socklen_t>(sizeof(sock))) != -1;
    }

    bool TcpV4ListenerSocket::listen(const int n){
        backlog_ = n;
        return ::listen(fdSocket_, backlog_) != -1;
    }

    TcpV4StreamSocket TcpV4ListenerSocket::acceptOne(){
        TcpV4StreamSocket ss;
        struct sockaddr_in cli;
        socklen_t cli_len;
        ss.fdSocket_ = accept(fdSocket_, (struct sockaddr*)&cli, &cli_len);
        if(ss.fdSocket_ == -1){
            //TODO: manage
            throw socket_error{};
        }
        ss.peer_ = sockaddrToString((struct sockaddr*)&cli);
        ss.peerPort_ = cli.sin_port;
        return ss;
    }

    bool TcpV6ListenerSocket::bind(const v6Addr& address, const Port& port){
        struct sockaddr_in6 sock;
        memset(&sock, 0, sizeof(sock));
        sock.sin6_addr = address;
        sock.sin6_port = htons(port);
        sock.sin6_family = toNative(domain_);

        return ::bind(fdSocket_, (struct sockaddr*)&sock,
            static_cast<socklen_t>(sizeof(sock))) != -1;
    }

    bool TcpV6ListenerSocket::listen(const int n){
        backlog_ = n;
        return ::listen(fdSocket_, backlog_) != -1;
    }

    TcpV6StreamSocket TcpV6ListenerSocket::acceptOne(){
        TcpV6StreamSocket ss;
        struct sockaddr_in6 cli;
        socklen_t cli_len;
        ss.fdSocket_ = accept(fdSocket_, (struct sockaddr*)&cli, &cli_len);
        if(ss.fdSocket_ == -1){
            //TODO: manage
            throw socket_error{};
        }
        ss.peer_ = sockaddrToString((struct sockaddr*)&cli);
        ss.peerPort_ = cli.sin6_port;
        //TODO: maybe the connection carries more info for ipv6
        return ss;
    }

    bool UdpV4ServerSocket::bind(const v4Addr& address, const Port& port){
        struct sockaddr_in sock;
        memset(&sock, 0, sizeof(sock));
        sock.sin_addr.s_addr = inet_addr(address.c_str());
        sock.sin_port = htons(port);
        sock.sin_family = toNative(domain_);

        return ::bind(fdSocket_, (struct sockaddr*)&sock,
            static_cast<socklen_t>(sizeof(sock))) != -1;
    }

    bool UdpV6ServerSocket::bind(const v6Addr& address, const Port& port){
        struct sockaddr_in6 sock;
        memset(&sock, 0, sizeof(sock));
        sock.sin6_addr = address;
        sock.sin6_port = htons(port);
        sock.sin6_family = toNative(domain_);

        return ::bind(fdSocket_, (struct sockaddr*)&sock,
            static_cast<socklen_t>(sizeof(sock))) != -1;
    }
}
