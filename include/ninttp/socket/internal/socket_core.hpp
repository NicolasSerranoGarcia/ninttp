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

#include <expected>
#include <iostream> //for std::cerr on SocketCore destructor
#include <type_traits>
#include <utility>

#include "backends/concepts.hpp"
#include "../types.hpp"
#include "../socket_error.hpp"
#include "socket_state.hpp"

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

            static_assert(std::swappable<SocketT>, "SocketT must be swappable");
            static_assert(std::is_move_constructible_v<SocketT>, "SocketT must be move constructible");
            static_assert(std::is_move_assignable_v<SocketT>, "SocketT must be move assignable");

            //for concurrency, init of the backend can be done with call_once
            constexpr SocketCore() noexcept
                : domain_(Domain::Invalid), service_(Service::Invalid), proto_(Protocol::Invalid), handle_(BackendT::invalidSocket())
            {
                #if NINTTP_SOCKET_BACKEND_REQUIRES_INIT == 1
                SocketCore::initBackend();
                #endif
            };

            //passing invalid casted values to any of the parameter types is undefined behavior
            SocketCore(Domain d, Service s, Protocol p)
                : domain_(d), service_(s), proto_(p)
            {
                #if NINTTP_SOCKET_BACKEND_REQUIRES_INIT == 1
                SocketCore::initBackend();
                #endif

                handle_ = BackendT::openSocket(d, s, p);
                if(!BackendT::isValidSocket(handle_))
                    throw SocketError{BackendT::getLastError()};
            }

            SocketCore(const SocketCore&) = delete;
            SocketCore& operator=(const SocketCore&) = delete;

            SocketCore(SocketCore&& other) noexcept(std::is_nothrow_move_constructible_v<SocketT>)
                : handle_(std::move(other.handle_)), domain_(std::move(other.domain_)), service_(std::move(other.service_)), proto_(std::move(other.proto_))
            {
                other.invalidate_();
            }

            //there is potential leak of resources here, because we are not disposing of the previous state
            SocketCore& operator=(SocketCore&& other) noexcept(std::is_nothrow_move_constructible_v<SocketT> && std::is_nothrow_swappable_v<SocketT>){
                //the destructor of the temporary left here might return an error but we can't handle it
                SocketCore(std::move(other)).swap(*this);
                return *this;
            }

            constexpr void swap(SocketCore& other) noexcept(std::is_nothrow_swappable_v<SocketT>){
                std::swap(domain_, other.domain_);
                std::swap(service_, other.service_);
                std::swap(proto_, other.proto_);
                std::swap(handle_, other.handle_);
            }

            //Only checks for backend validity. it can happen that the socket is valid, but it is not usable nor opened (for external causes)
            constexpr bool isValid() const noexcept{ return BackendT::isValidSocket(handle_); }

            //TODO: enforce conditions on socket state (thus implementing isOpen and isUsable) (invariants)

            //currently the same as isValid
            constexpr bool isUsable() const noexcept{ return isValid(); };

            //currently the same as isValid
            constexpr bool isOpen() const noexcept{ return isValid(); };

            constexpr Domain domain() const noexcept{ return domain_; };

            constexpr Service service() const noexcept{ return service_; };

            constexpr Protocol protocol() const noexcept{ return proto_; };

            // Return a read-only view of the native handle. Do not acquire it by
            // another owner unless you first release() this SocketCore.
            constexpr const SocketT& nativeHandle() const noexcept{ return handle_; };

            std::expected<void, SocketError> shutdown(ShutdownPolicy what) noexcept{
                if(BackendT::shutdownSocket(this->handle_, what)){
                    return {};
                }

                return std::unexpected{SocketError{BackendT::getLastError()}};
            }

            //closes previous state. May return an error if closing didn't work. Does not change state if so (strong guarantee).
            //currently it does not. It may close the previous socket without creating a new one (openSocket may fail after closing the previous).
            //maybe preopen a socket, and if it fails return the error, and only then close the previous?
            //but if closing fails, we will need to close the preallocated socket, but that can also fail, leaving us with two error codes (failed closing the previous
            //and failed closing the preallocated). Look into this
            std::expected<void, SocketError> open(Domain d, Service s, Protocol p) noexcept{
                if(auto e = this->close(); !e.has_value())
                    return std::unexpected{e.error()};

                handle_ = BackendT::openSocket(d, s, p);
                if(!BackendT::isValidSocket(handle_))
                    return std::unexpected{SocketError{BackendT::getLastError()}};

                domain_ = d;
                service_ = s;
                proto_ = p;

                return {};
            }

            [[nodiscard]] constexpr SocketT release() noexcept(std::is_nothrow_move_constructible_v<SocketT> && std::is_nothrow_move_assignable_v<SocketT>){
                SocketT prev = std::move(handle_);
                this->invalidate_();
                return prev;
            }

            //explicit cleanup, better for handling than the destructor
            //invalidates the instance. 
            std::expected<void, SocketError> close() noexcept{
                //already closed or invalidated somewhere else
                if(!BackendT::isValidSocket(handle_))
                    return {};

                if(BackendT::closeSocket(handle_)){
                    this->invalidate_();
                    return {};
                }

                return std::unexpected{SocketError{BackendT::getLastError()}};
            }

            //please be aware of letting the destructor handle shutdown of the socket instead of manually calling close()
            virtual ~SocketCore() noexcept{
                //close returned an error
                if(auto e = this->close(); !e.has_value())
                    std::cerr << e.error().msg() << std::endl;
            };

        protected:
            SocketT handle_;
            Domain domain_;
            Service service_;
            Protocol proto_;
            SocketState state_;

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
            //here one would expect returning an optional error code, but as this is always used inside a constructor,
            //the final output will still be a throw if something goe wrong, so just let it bubble up to the caller.
            static void initBackend(){
                if(!backendInited){
                    if(!BackendT::init())
                        throw SocketError{BackendT::getLastError()};
                    backendInited = true;
                }
            }
            static inline constinit bool backendInited = false;
            #endif
    };
} // namespace ninttp::internal
