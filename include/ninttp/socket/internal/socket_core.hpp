/**
 * @file socket_core.hpp
 * @author Nicolás Serrano (serranogarcianicolas@gmail.com)
 * @brief defines a generic socketCore, taking in a backend as a template.
 * @version 0.1
 * @date 2026-04-08
 * 
 * @copyright Copyright (c) 2026 Nicolás Serrano García
 * 
 */

#pragma once

#include <cassert>
#include <expected>
#include <iostream> //for std::cerr on SocketCore destructor
#include <type_traits>
#include <utility>

#include "backends/concepts.hpp"
#include "../types.hpp"
#include "../socket_error.hpp"

//for future: it may take too long on multithreaded programs to capture errno. For that, we could maybe do a "recordLastError" so that you can presave the
//error, just for handling it in subsequent steps. The message could be created from this memory, just like getErrMessageFromCapture. This way we avoid
//mixing up errors. 

namespace ninttp::internal
{
    //on this level of abstraction, any socket can be interchanged, moved from to others. Here we define move, swap, release, acquire, but that doesn't mean that 
    //on higher levels they can do the same
    //Semantics: a variable of this type can be reused to hold multiple valid instances of a socket representation. 
    //the directives "open", "close" and "release" enable for multiple usage throughout the lifetime. One can also move 
    //an instance to another (which would be an equivalent to an "adopt" method, but as a SocketT has a Domain, 
    //Service and Protocol associated, this later operation would essentially be a move operation, which is why it is not present in the API and
    //move operator is encouraged instead for transfering ownership)
    template <SocketBackend BackendT>
    class SocketCore{
        public:
            using SocketT = typename BackendT::SocketT;
            using CloseStatus = SocketCloseStatus<SocketError>;

            static_assert(std::is_nothrow_swappable_v<SocketT>, "SocketT must be nothrow swappable");
            static_assert(std::is_nothrow_move_constructible_v<SocketT>, "SocketT must be nothrow move constructible");
            static_assert(std::is_nothrow_move_assignable_v<SocketT>, "SocketT must be nothrow move assignable");

            //does not start a local descriptor, just like std::thread does
            SocketCore()
                : handle_(BackendT::invalidSocket()), domain_(Domain::Invalid), service_(Service::Invalid), proto_(Protocol::Invalid)
            {
                #if NINTTP_SOCKET_BACKEND_REQUIRES_INIT == 1
                if(auto initialized = SocketCore::initBackend(); !initialized.has_value())
                    throw initialized.error();
                #endif
            };

            //passing invalid casted values to any of the parameter types is undefined behavior
            SocketCore(Domain d, Service s, Protocol p)
                : handle_(BackendT::invalidSocket()), domain_(d), service_(s), proto_(p)
            {
                #if NINTTP_SOCKET_BACKEND_REQUIRES_INIT == 1
                if(auto initialized = SocketCore::initBackend(); !initialized.has_value())
                    throw initialized.error();
                #endif

                auto opened = BackendT::openSocket(d, s, p);
                if(!opened.has_value())
                    throw SocketError{opened.error()};

                handle_ = std::move(*opened);
            }

            SocketCore(const SocketCore&) = delete;
            SocketCore& operator=(const SocketCore&) = delete;

            SocketCore(SocketCore&& other) noexcept
                : handle_(std::move(other.handle_)), domain_(std::move(other.domain_)), service_(std::move(other.service_)), proto_(std::move(other.proto_))
            {
                other.invalidate_();
            }

            //there is potential leak of resources here, because we are not disposing of the previous state
            //the destructor of the temporary left here might return an error but we can't handle it
            SocketCore& operator=(SocketCore&& other) noexcept{
                SocketCore(std::move(other)).swap(*this);
                return *this;
            }

            constexpr void swap(SocketCore& other) noexcept{
                std::swap(domain_, other.domain_);
                std::swap(service_, other.service_);
                std::swap(proto_, other.proto_);
                std::swap(handle_, other.handle_);
            }

            //Only checks for backend validity. it can happen that the socket is valid, but it is not usable nor opened (for external causes)
            constexpr bool isUsable() const noexcept{ return BackendT::isUsableSocket(handle_); }

            constexpr Domain domain() const noexcept{ return domain_; };

            constexpr Service service() const noexcept{ return service_; };

            constexpr Protocol protocol() const noexcept{ return proto_; };

            std::expected<void, SocketError> shutdown(ShutdownPolicy what) noexcept{
                auto shutdown = BackendT::shutdownSocket(this->handle_, what);
                if(!shutdown.has_value())
                    return std::unexpected{SocketError{shutdown.error()}};

                return {};
            }

            //replacement for move assignment that explicitly returns the state of closing the descriptor of the previous state
            //If closing the previous socket state fails, the other parameter is NOT consumed and the caller retains its lifetime
            std::expected<void, CloseStatus> replace(SocketCore&& other){
                if(auto closed = this->close(); !closed){
                    return std::unexpected{closed.error()};
                }

                *this = std::move(other);
                return {};
            }

            [[nodiscard]] constexpr SocketT release() noexcept{
                SocketT prev = std::move(handle_);
                this->invalidate_();
                return prev;
            }

            //explicit cleanup, better for handling than the destructor
            //Retains ownership only when the backend explicitly permits retrying.
            std::expected<void, CloseStatus> close() noexcept{
                //already closed or invalidated somewhere else
                if(!BackendT::isUsableSocket(handle_))
                    return {};

                auto status = BackendT::closeSocket(handle_);

                if(status.disposition != SocketCloseDisposition::Retry)
                    this->invalidate_();

                if(status.error.has_value())
                    return std::unexpected{CloseStatus{
                        .disposition = status.disposition,
                        .error = SocketError{*status.error}
                    }};

                assert(status.disposition == SocketCloseDisposition::Released);
                return {};
            }

            //please be aware of letting the destructor handle shutdown of the socket instead of manually calling close()
            virtual ~SocketCore() noexcept{
                //close returned an error
                if(auto closed = this->close(); !closed.has_value() && closed.error().error.has_value())
                    std::cerr << closed.error().error->msg() << std::endl;
            };

        protected:
            SocketT handle_;
            Domain domain_;
            Service service_;
            Protocol proto_;

            //a combination of acquire and open which would be more error prone
            //for public API. It is only used for child socketClasses (like ListenerSocket) to build
            //a socket that has a socketT already bound to the OS, but that in order to correctly
            //construct it's wrapper, the construction information is needed. This is why the domain,
            //service and protocol are taken in.
            constexpr SocketCore(SocketT sock, Domain d, Service s, Protocol p) noexcept 
                : handle_(sock), domain_(d), service_(s), proto_(p){}

        private:

            //does not release the handle. Use with care even yet it being private
            void invalidate_() noexcept{
                handle_ = BackendT::invalidSocket();
                domain_ = Domain::Invalid;
                proto_ = Protocol::Invalid;
                service_ = Service::Invalid;
            }
            
            #if NINTTP_SOCKET_BACKEND_REQUIRES_INIT == 1
                static std::expected<void, SocketError> initBackend() noexcept{
                    if(backendInited)
                        return {};

                    auto initialized = BackendT::init();
                    if(!initialized.has_value())
                        return std::unexpected{SocketError{initialized.error()}};

                    backendInited = true;
                    return {};
                }
                static inline constinit bool backendInited = false;

                friend std::expected<void, SocketError> ninttp::deinitBackend() noexcept;
            #endif
    };
} // namespace ninttp::internal

namespace ninttp {
    #if NINTTP_SOCKET_BACKEND_REQUIRES_INIT == 1
    //this function is completely optional (and personally I would not use it) unless for very specific reasons. One of them might be
    //that you are on windows, using Winsock, and you are doing additional OS API level programming, and you need fine grained control 
    //over the availability of the winsock dll. Otherwise (your program needs ninttp until closing) it will get cleaned up and no resources will
    //be leaked
    inline std::expected<void, SocketError> deinitBackend() noexcept{
        if(!SocketBase::backendInited)
            return {};

        auto deinitialized = internal::SelectedBackend::deinit();
        if(deinitialized.has_value()){
            SocketBase::backendInited = false;
            return {};
        }

        return std::unexpected{SocketError{deinitialized.error()}};
    }
    #endif
} // namespace ninttp
