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
#include <cstdio>
#include <expected>
#include <type_traits>
#include <utility>
#include <mutex>

#include "backends/concepts.hpp"
#include "../types.hpp"
#include "../socket_error.hpp"

//for future: it may take too long on multithreaded programs to capture errno. For that, we could maybe do a "recordLastError" so that you can presave the
//error, just for handling it in subsequent steps. The message could be created from this memory, just like getErrMessageFromCapture. This way we avoid
//mixing up errors.
namespace ninttp::internal
{
    #if NINTTP_SOCKET_BACKEND_REQUIRES_INIT == 1
    constinit inline std::once_flag backendInitOnce{};
    constinit inline bool backendInited = false;
    #endif

    //on this level of abstraction, any socket can be interchanged, moved from to others. Here we define move, swap, release, acquire, but that doesn't mean that 
    //on higher levels they can do the same
    //Semantics: a variable of this type can be reused to hold multiple valid instances of a socket representation. 
    //the directives "open", "close" and "release" enable for multiple usage throughout the lifetime. One can also move 
    //an instance to another, which would be an equivalent to an "adopt" method, present on the API. This latter is preferred over the nonthrowing move operator, for the reason
    //being that one can handle the possible error from destroying the previous state of the socket before swapping it. This is better explained in its correspondent methods
    template <SocketBackend BackendT>
    class SocketCore{
        public:
            using SocketT = typename BackendT::SocketT;
            using CloseStatus = SocketCloseStatus<SocketError>;

            static_assert(std::is_nothrow_swappable_v<SocketT>, "SocketT must be nothrow swappable");
            static_assert(std::is_nothrow_move_constructible_v<SocketT>, "SocketT must be nothrow move constructible");
            static_assert(std::is_nothrow_move_assignable_v<SocketT>, "SocketT must be nothrow move assignable");

            /**
             * @brief Construct an empty socket core.
             *
             * Does not start a local descriptor, just like std::thread does.
             *
             * @throws SocketError If the selected backend requires initialization and that
             * initialization fails.
             */
            SocketCore()
                : handle_(BackendT::invalidSocket()), domain_(Domain::Invalid), service_(Service::Invalid), proto_(Protocol::Invalid)
            {
                #if NINTTP_SOCKET_BACKEND_REQUIRES_INIT == 1
                SocketCore::initBackend();
                #endif
            };

            /**
             * @brief Construct and open a native socket.
             *
             * Passing invalid casted values to any of the parameter types is undefined behavior.
             *
             * @throws SocketError If backend initialization fails, or if opening the native
             * socket fails.
             */
            SocketCore(Domain d, Service s, Protocol p)
                : handle_(BackendT::invalidSocket()), domain_(d), service_(s), proto_(p)
            {
                #if NINTTP_SOCKET_BACKEND_REQUIRES_INIT == 1
                SocketCore::initBackend();
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

            /**
             * @brief Unchecked replacement, similar in spirit to unchecked std container operations.
             *
             * This operator stays noexcept and always consumes @p other, but the previous socket
             * state is left to the temporary destructor. If that destructor cannot actually close the
             * previous native socket, the failure cannot be reported here and the
             * target may leak.
             *
             * @warning Use replace() when the close result of the previous state must be observed
             * by the caller.
             *
             * @param other Socket core whose state will be moved into this object.
             * @return Reference to this object.
             */
            SocketCore& operator=(SocketCore&& other) noexcept{
                SocketCore(std::move(other)).swap(*this);
                return *this;
            }

            /**
             * @brief Swaps native handles and socket metadata.
             *
             * @param other Socket core to swap with.
             */
            constexpr void swap(SocketCore& other) noexcept{
                std::swap(domain_, other.domain_);
                std::swap(service_, other.service_);
                std::swap(proto_, other.proto_);
                std::swap(handle_, other.handle_);
            }

            /**
             * @brief Checks whether the handle is not the backend invalid sentinel.
             *
             * This only checks the local handle value. It does not prove that the operating
             * system still considers the socket connected, open, or otherwise usable.
             */
            [[nodiscard]] constexpr bool isUsable() const noexcept{ return BackendT::isUsableSocket(handle_); }

            /**
             * @brief Returns the socket domain recorded when the handle was opened or adopted.
             */
            [[nodiscard]] constexpr Domain domain() const noexcept{ return domain_; };

            /**
             * @brief Returns the socket service recorded when the handle was opened or adopted.
             */
            [[nodiscard]] constexpr Service service() const noexcept{ return service_; };

            /**
             * @brief Returns the socket protocol recorded when the handle was opened or adopted.
             */
            [[nodiscard]] constexpr Protocol protocol() const noexcept{ return proto_; };

            /**
             * @brief Shuts down receive, send, or both directions of the socket.
             *
             * @param what Direction policy passed to the selected backend.
             * @return Empty result on success, or SocketError wrapping the native shutdown error.
             */
            [[nodiscard]] std::expected<void, SocketError> shutdown(ShutdownPolicy what) noexcept{
                auto shutdown = BackendT::shutdownSocket(this->handle_, what);
                if(!shutdown.has_value())
                    return std::unexpected{SocketError{shutdown.error()}};

                return {};
            }

            /**
             * @brief Checked replacement for move assignment.
             *
             * The current socket state is closed first and its close status is returned on
             * failure. In that case @p other is not consumed, so the caller retains ownership and
             * can decide how to handle the failed cleanup.
             *
             * @param other Socket core whose state will replace the current one after successful
             * cleanup.
             * @return Empty result on success, or the close status of the current state on failure.
             */
            [[nodiscard]] std::expected<void, CloseStatus> replace(SocketCore&& other){
                if(auto closed = this->close(); !closed){
                    return std::unexpected{closed.error()};
                }

                //here it is okay to use the move assignment because when .close() succeeds, it invalidates the socket, meaning 
                //that the call in the destructor will have no effect which is what can trigger the leak on the move assignment.
                *this = std::move(other);
                return {};
            }

            /**
             * @brief Releases ownership of the native handle without closing it.
             *
             * The socket core is invalidated after release. The caller becomes responsible for
             * closing the returned native handle according to the selected backend rules.
             *
             * @return Previously owned native handle.
             */
            [[nodiscard]] constexpr SocketT release() noexcept{
                SocketT prev = std::move(handle_);
                this->invalidate_();
                return prev;
            }

            /**
             * @brief Explicit cleanup, better for handling than the destructor.
             *
             * This is the preferred method to handle graceful close of the socket. This does not
             * follow a strict RAII approach, but it is what we get when trying to encapsulate a
             * descriptor state into a lifetime object. Closing a socket can fail in various ways,
             * but the key takeaway is which state the socket is in when the close operation fails.
             *
             * This function only reports the state at the moment of call. This means that the
             * actual policy for close errors, such as what to do on Retry, is up to the user of
             * this class. Ownership is retained only when the backend explicitly permits retrying.
             *
             * @return Empty result on success, or the close status reported by the backend on
             * failure.
             *
             * @see SocketCloseDisposition
             */
            [[nodiscard]] std::expected<void, CloseStatus> close() noexcept{
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

            /**
             * @brief Best-effort socket cleanup.
             *
             * Please be aware of letting the destructor handle shutdown of the socket instead of
             * manually calling close(). Errors are reported through C stdio to avoid throwing from
             * a noexcept destructor while diagnostics are emitted.
             */
            virtual ~SocketCore() noexcept{
                auto closed = this->close();

                if(!closed.has_value() && closed.error().error.has_value()){
                    std::fprintf(
                        stderr,
                        "Failed to close socket during destruction. Native error: %d\n",
                        static_cast<int>(closed.error().error->errorCode()));
                }
            };

        protected:
            SocketT handle_;
            Domain domain_;
            Service service_;
            Protocol proto_;

            /**
             * @brief Adopts an already-open native socket and its recorded metadata.
             *
             * This constructor is protected because public unchecked adoption would be easy to
             * misuse. ListenerSocket uses it to construct connected sockets returned by accept().
             */
            constexpr SocketCore(SocketT sock, Domain d, Service s, Protocol p) noexcept 
                : handle_(sock), domain_(d), service_(s), proto_(p){}

        private:
            /**
             * @brief Marks this core as empty without closing the current handle.
             */
            void invalidate_() noexcept{
                handle_ = BackendT::invalidSocket();
                domain_ = Domain::Invalid;
                proto_ = Protocol::Invalid;
                service_ = Service::Invalid;
            }
            
            #if NINTTP_SOCKET_BACKEND_REQUIRES_INIT == 1
            /**
             * @brief Initialize the selected backend once for the process.
             *
             * std::call_once marks the backend as initialized only if the initialization callable
             * returns normally. If backend initialization fails, this function throws and a later
             * socket construction may retry initialization.
             *
             * @throws SocketError If backend initialization fails.
             */
            static void initBackend(){
                std::call_once(backendInitOnce, []{
                    auto initialized = BackendT::init();
                    if(!initialized.has_value())
                        throw SocketError{initialized.error()};

                    backendInited = true;
                });
            }
            #endif
    };
} // namespace ninttp::internal

#if NINTTP_SOCKET_BACKEND_REQUIRES_INIT == 1
namespace ninttp {
    /**
     * @brief Deinitialize the socket backend after explicit backend lifetime management.
     *
     * This function is optional and intended for specific cases where the program needs fine
     * grained control over the backend lifetime, such as direct Winsock API use on Windows.
     *
     * - Do not call this while any ninttp socket objects are alive.
     * - Do not call this concurrently with socket construction or other deinitialization calls.
     * - Backend initialization is one-shot; after successful deinitialization, ninttp will not
     *   reinitialize the backend in the same process because the std::once_flag cannot be reset.
     *
     * TODO: Consider replacing this manual contract with reference-counted backend lifetime.
     *
     * @return Empty result on success, or the backend deinitialization error.
     */
    [[nodiscard]] inline std::expected<void, SocketError> deinitBackend() noexcept{
        if(!internal::backendInited)
            return {};

        auto deinitialized = internal::SelectedBackend::deinit();
        if(deinitialized.has_value()){
            internal::backendInited = false;
            return {};
        }

        return std::unexpected{SocketError{deinitialized.error()}};
    }
} // namespace ninttp
#endif
