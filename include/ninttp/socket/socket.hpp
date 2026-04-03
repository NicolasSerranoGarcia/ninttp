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
#include <arpa/inet.h>

#include <string>
#include <array>
#include <algorithm>
#include <type_traits>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <iostream>

#include "interfaces.hpp"
#include "types.hpp"
#include "errors.hpp"

//current TODO: move things to different headers. Inspect and re-build the Ipv4 endpoint to understand how it works.
//Add concepts and implement IEndpoint class. Document methods. Go through socketBase and StreamSocket and look for TODOs
//strengthen the interface with more detailed errors:
//
//add c++ semantics

namespace ninttp
{

    //TODO: semantics of sockets? one time use like std::thread but can be reused as a var?
    //can close connection any time?

    class SocketBase{
        public:
            SocketBase() noexcept 
                : domain_(Domain::Invalid), service_(Service::Invalid), protocol_(Protocol::Invalid), fdSocket_(-1) {};

            SocketBase(Domain domain, Service service, Protocol protocol = Protocol::Default)
                : domain_(domain), service_(service), protocol_(protocol), fdSocket_(-1)
            {
                const int nativeDomain = toNative(domain_);
                const int nativeService = toNative(service_);
                const int nativeProtocol = toNative(protocol_);

                if (nativeDomain < 0){
                    throw PolicyError{PolicyErrCode::UNRECOGNIZED_DOMAIN};
                }

                if (nativeService < 0){
                    throw PolicyError{PolicyErrCode::UNRECOGNIZED_SERVICE};
                }

                if (nativeProtocol < 0){
                    throw PolicyError{PolicyErrCode::UNRECOGNIZED_PROTOCOL};
                }

                fdSocket_ = ::socket(nativeDomain, nativeService, nativeProtocol);

                if(fdSocket_ == -1){
                    throw socketError{strerror(errno)};
                }
            }

            SocketBase(int nativeDomain, int nativeService, int nativeProtocol)
                : SocketBase(toDomain(nativeDomain), toService(nativeService), toProtocol(nativeProtocol)){};

            SocketBase(const SocketBase&) = delete;
            SocketBase& operator=(const SocketBase&) = delete;

            SocketBase(SocketBase&& other) noexcept 
                : domain_(std::move(other.domain_)), service_(std::move(other.service_)), protocol_(std::move(other.protocol_)), fdSocket_(std::move(other.fdSocket_)){
                other.domain_ = Domain::Invalid;
                other.fdSocket_ = -1;
                other.protocol_ = Protocol::Invalid;
                other.service_ = Service::Invalid;
            }

            SocketBase& operator=(SocketBase&& other) noexcept{
                SocketBase(std::move(other)).swap(*this);
                return *this;
            }

            bool isOpen() const noexcept{ return fdSocket_ != -1; }

            void swap(SocketBase& other) noexcept{
                std::swap(domain_, other.domain_);
                std::swap(service_, other.service_);
                std::swap(protocol_, other.protocol_);
                std::swap(fdSocket_, other.fdSocket_);
            }

            Domain domain() const noexcept{ return domain_; };

            Service service() const noexcept{ return service_; };

            Protocol protocol() const noexcept{ return protocol_; };

            int nativeHandle() const noexcept{ return fdSocket_; };

            int release() noexcept{
                int prev = -1;
                std::swap(fdSocket_, prev);
                return prev;
            }

            //::shutdown can (and should) return ENOTCONN when this is of SocketBase type
            //bc SocketBase only gives basic common interface, but it is not usable.
            //This is why it doesn't make sense to call it from a SocketBase this
            void shutdown(ShutdownPolicy what) const{
                auto native = toNativeShutdownPolicy(what);
                if(native == -1){
                    throw PolicyError(PolicyErrCode::UNRECOGNIZED_SHUTDOWN_POLICY);
                }

                if(::shutdown(fdSocket_, native) == -1){
                    throw socketError(strerror(errno));
                }
            }

            //explicit cleanup, better for handling than the destructor
            void close(){
                //already closed
                if(fdSocket_ == -1){
                    return;
                }

                if(::close(fdSocket_) == 0){
                    fdSocket_ = -1;
                    return;
                }

                throw socketError(strerror(errno));
            }

            virtual ~SocketBase() noexcept{
                try{
                    close();
                } catch(socketError& e){
                    std::cerr << e.msg() << std::endl;
                }
            };

        protected:
            Domain domain_;
            Service service_;
            Protocol protocol_;
            int fdSocket_;

            //constructor for children classes that avoids creating a new socket
            SocketBase(int fd, Domain domain, Service service, Protocol protocol) noexcept 
                : domain_(domain), service_(service), protocol_(protocol), fdSocket_(fd){}
    };

    //Endpoint interface recap. must be convertible to sockaddr*, must have a .size()
    //must be constructible from the info that ::accept() returns

    //ConnectedSocketT recap. must be constructible from what ::accept() returns overall

    //enforce this not on the created endpoint classes but rather on the callee with concepts?

    template <typename EndpointT, typename ConnectedSocketT>
    class ListenerSocket : public SocketBase, public IBindable<EndpointT>, public IListener<ConnectedSocketT>{
        public:
            ListenerSocket(Domain domain, Protocol protocol)
                : SocketBase(domain, Service::Stream, protocol){}

            //bind a specific interface of the server interface to this socket
            //Endpoint must be convertible to sockaddr* and we must be able to take it's size
            //enforce that with a concept? in the Java way
            void bind(const EndpointT& endpoint) override {
                if(::bind(fdSocket_, endpoint, endpoint.size()) != 0)
                    throw socketError(strerror(errno));
            }

            void listen(const int n) override{
                if(::listen(fdSocket_, this->backlog_ = n) != 0)
                    throw socketError(strerror(errno));
            };

            //delegate on the constructor of ConnectedSocketT what ::accept() returns
            ConnectedSocketT accept() override{
                //pass in sockaddr_storage
                struct sockaddr_storage cli{};
                socklen_t cli_len = static_cast<socklen_t>(sizeof(cli));
                auto fd = ::accept(fdSocket_, (struct sockaddr*)&cli, &cli_len);
                if(fd == -1){
                    throw socketError{strerror(errno)};
                }

                //all connected sockets must have this method (mainly stream sockets and seqpacket sockets)
                return ConnectedSocketT::fromAccepted(fd, this->domain_, this->protocol_, cli);
            };
    };

    template <typename EndpointT>
    class StreamSocket : public SocketBase, public IConnectable<EndpointT>, public IStreamIO{
        public:

            //when building it from scratch, this ctor then connect
            StreamSocket(Domain domain, Protocol protocol) : SocketBase(domain, Service::Stream, protocol){};

            static StreamSocket fromAccepted(
                int fd,
                Domain domain,
                Protocol protocol,
                const sockaddr_storage& peerStorage)
            {
                return StreamSocket(fd, domain, protocol, EndpointT(peerStorage));
            }

            //connect enforces the notion that an endpoint must be convertible to sockaddr*
            void connect(const EndpointT& endpoint) override{
                if(::connect(fdSocket_, endpoint_ = endpoint, endpoint.size()) != 0)
                    throw socketError(strerror(errno));
            }

            //create second connect method that takes an rvalue so that std::move() would avoid a copy

            //send len bytes from buffer to endpoint. Return the number of bytes sent or throw socketError on error
            size_t send(const char* buffer, size_t len) override{
                if(buffer == nullptr) return 0;

                if(auto sent = ::send(fdSocket_, buffer, len, 0); sent != -1){
                    return sent;
                }

                throw socketError{strerror(errno)};
            };

            size_t receive(char* buffer, size_t n) override{
                if(buffer == nullptr) return 0;

                if(auto recv = ::recv(fdSocket_, buffer, n, 0); recv != -1){
                    return recv;
                }

                throw socketError{strerror(errno)};
            };

        private:
            EndpointT endpoint_;

            StreamSocket(int fd, Domain domain, Protocol protocol, const EndpointT& endpoint)
                : SocketBase(fd, domain, Service::Stream, protocol), endpoint_(endpoint){};
    };

    //this is the client 
    // template <typename SendEndpointT, typename RecvEndpointT>
    // class DatagramSocket : public SocketBase, public IDatagramIO<SendEndpointT, RecvEndpointT>{
    //     public:

    //         DatagramSocket(){}

    //         ssize_t sendTo(const SendEndpointT& endpoint, const char* buffer, size_t len) override{
    //             if(buffer == nullptr) return 0;

    //             //endpoint is implicitly casted
    //             if(auto recv = ::sendto(fdSocket_, buffer, len, 0, endpoint, endpoint.size()); recv != -1){
    //                 return recv;
    //             }

    //             throw socketError{"StreamSocket::receive(...): ", errno};
    //         };

    //         ssize_t recvFrom(char* buffer, size_t len) override{
    //             // if(buffer == nullptr) return 0;

    //             // //endpoint is implicitly casted
    //             // if(auto recv = ::recvfrom(fdSocket_, buffer, len, 0, endpoint, endpoint.size()); recv != -1){
    //             //     return recv;
    //             // }

    //             // throw socketError{"StreamSocket::receive(...): ", errno};
    //         };
    // };
} // namespace ninttp
