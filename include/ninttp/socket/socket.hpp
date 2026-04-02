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
#include <type_traits>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <iostream>

#include "interfaces.hpp"

//move some things here
#include "types.hpp"

//current TODO: move things to different headers. Inspect and re-build the Ipv4 endpoint to understand how it works.
//Add concepts and implement IEndpoint class. Document methods. Go through socketBase and StreamSocket and look for TODOs
//strengthen the interface with more detailed errors:
#include <source_location>
//
//add c++ semantics

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

    //TODO: semantics of sockets? one time use like std::thread but can be reused as a var?
    //can close connection any time?

    //.msg() can be verbose,so most of the times, if you want to handle the error, you may want to consult the nativeError_ code
    //which on unix platforms comes from errno, and handle each individually
    struct socketError{
        socketError(const char* context, int nativeErr) noexcept : context(context), nativeError_(nativeErr) {};

        std::string msg() const noexcept{
            //this is going to fail bc of the ()
            return context + strerror(nativeError_);
        }

        std::string context;
        int nativeError_;
    };

    enum PolicyErrCode{
        UNKNOWN,
        UNRECOGNIZED_DOMAIN,
        UNRECOGNIZED_SERVICE,
        UNRECOGNIZED_PROTOCOL,
        UNRECOGNIZED_SHUTDOWN_POLICY
    };

    inline std::string strFromPolicyError(int pErr) noexcept{
        switch(pErr){
            case UNRECOGNIZED_DOMAIN:
                return "unrecognized domain";
            case UNRECOGNIZED_SERVICE:
                return "unrecognized service";
            case UNRECOGNIZED_PROTOCOL:
                return "unrecognized protocol";
            case UNRECOGNIZED_SHUTDOWN_POLICY:
                return "unrecognized socket shutdown policy";
            default:
                return "unknown policy error";
        }
    }

    enum class ShutdownPolicy{
        SHUT_RECEPTIONS,
        SHUT_TRANSMISSIONS,
        SHUT_TRANSMISSIONS_AND_RECEPTIONS
    };

    int toNativeShutdownPolicy(ShutdownPolicy what){
        switch(what){
            case ShutdownPolicy::SHUT_RECEPTIONS:
                return SHUT_RD;
            case ShutdownPolicy::SHUT_TRANSMISSIONS:
                return SHUT_WR;
            case ShutdownPolicy::SHUT_TRANSMISSIONS_AND_RECEPTIONS:
                return SHUT_RDWR;
            default:
                return -1;
        }
    }

    struct PolicyError{
        PolicyError(const char* context, int policyErr) noexcept : context(context), policyError_(policyErr) {};

        std::string msg() const noexcept{
            return context + strFromPolicyError(policyError_);
        }

        std::string context;
        int policyError_;
    };

    class SocketBase{
        public:
            SocketBase() noexcept : domain_(Domain::Invalid), service_(Service::Invalid), protocol_(Protocol::Invalid), fdSocket_(-1) {};

            SocketBase(Domain domain, Service service, Protocol protocol)
                : domain_(domain), service_(service), protocol_(protocol), fdSocket_(-1){
                const int nativeDomain = toNative(domain_);
                const int nativeService = toNative(service_);
                const int nativeProtocol = toNative(protocol_);

                if (nativeDomain < 0){
                    throw PolicyError{"SocketBase(Domain, Service, Protocol): ", UNRECOGNIZED_DOMAIN};
                }

                if (nativeService < 0){
                    throw PolicyError{"SocketBase(Domain, Service, Protocol): ", UNRECOGNIZED_SERVICE};
                }

                if (nativeProtocol < 0){
                    throw PolicyError{"SocketBase(Domain, Service, Protocol): ", UNRECOGNIZED_PROTOCOL};
                }

                fdSocket_ = ::socket(nativeDomain, nativeService, nativeProtocol);

                if(fdSocket_ == -1){
                    throw socketError{"SocketBase(Domain, Service, Protocol): ", errno};
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
                    throw PolicyError("SocketBase::shutdown(ShutdownPolicy): ", UNRECOGNIZED_SHUTDOWN_POLICY);
                }

                if(::shutdown(fdSocket_, native) == -1){
                    throw socketError("SocketBase::shutdown(ShutdownPolicy): ", errno);
                }
            }

            //explicit cleanup, better for handling than the destructor
            void close(){
                //already closed
                if(fdSocket_ == -1){
                    return;
                }

                auto closed = ::close(fdSocket_) == 0;

                if(closed){
                    fdSocket_ = -1;
                    return;
                }

                throw socketError("SocketBase::close(): ", errno);
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
                    throw socketError("ListenerSocket::bind(const EndpointT&): ", errno);
            }

            void listen(const int n) override{
                if(::listen(fdSocket_, this->backlog_ = n) != 0)
                    throw socketError("ListenerSocket::listen(const int): ", errno);
            };

            //delegate on the constructor of ConnectedSocketT what ::accept() returns
            ConnectedSocketT accept() override{
                //pass in sockaddr_storage
                struct sockaddr_storage cli{};
                socklen_t cli_len = static_cast<socklen_t>(sizeof(cli));
                auto fd = ::accept(fdSocket_, (struct sockaddr*)&cli, &cli_len);
                if(fd == -1){
                    throw socketError{"ListenerSocket::accept(): ", errno};
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
                    throw socketError("StreamSocket::connect(const EndpointT&): ", errno);
            }

            //create second connect method that takes an rvalue so that std::move() would avoid a copy

            //send len bytes from buffer to endpoint. Return the number of bytes sent or throw socketError on error
            size_t send(const char* buffer, size_t len) override{
                if(buffer == nullptr) return 0;

                if(auto sent = ::send(fdSocket_, buffer, len, 0); sent != -1){
                    return sent;
                }

                throw socketError{"StreamSocket::send(...): ", errno};
            };

            size_t receive(char* buffer, size_t n) override{
                if(buffer == nullptr) return 0;

                if(auto recv = ::recv(fdSocket_, buffer, n, 0); recv != -1){
                    return recv;
                }

                throw socketError{"StreamSocket::receive(...): ", errno};
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
