#pragma once

#include <concepts>
#include <type_traits>
#include <utility>
#include <optional>
#include <string>

#include "../../types.hpp"
#include "../../../nintraits.hpp"

namespace ninttp::internal {
    /**
     * @brief A concept that must be satisfied by all socket backends
     * @note As the C++ standard states, concepts should check semantical correctness. This concept just checks for structure of the class. It is closer to a Java interface rather
     * than a concept. We cannot explicitly check that, if a class fulfills this concept, that any of the operations are semantically or conceptually correct. I believe this concept
     * is more of a debug/compiler check to make sure all backends implement the same interface. The semantics come from the intent of the names itself. we cannot possibly
     * check for semantic correctness, mainly because behavior differs from platform to platform. It is up to the implementer to make the most correct semantic implementation
     * of the method
     * @details An implementation of a backend should not use throw mechanisms to communicate failed operations, but rather use the getLastError
     * 
     * @tparam Backend 
     */
    template <typename Backend>
    concept SocketBackend =
        requires {
            typename Backend::SocketT;
            typename Backend::ErrorT;
            typename Backend::AddressStorageT;
            typename Backend::AddressLenT;

            //a struct to return a combination of SocketT, AddressStorageT and AddressLenT for accept()
            typename Backend::AddressBundleT;

        }&& requires(typename Backend::AddressBundleT bundle) {
            requires std::same_as<
                std::remove_cvref_t<decltype(bundle.socket)>,
                typename Backend::SocketT>;

            requires std::same_as<
                std::remove_cvref_t<decltype(bundle.storage)>,
                typename Backend::AddressStorageT>;

            requires std::same_as<
                std::remove_cvref_t<decltype(bundle.len)>,
                typename Backend::AddressLenT>;
        }
        && std::swappable<typename Backend::SocketT>
        && std::is_move_constructible_v<typename Backend::SocketT>
        && std::is_move_assignable_v<typename Backend::SocketT>
        && requires {
            /**
             * @brief The backend representation of an invalid socket. User is encouraged to use invalidSocket() most of the times
             * @note Only use this if you know your desired backend has a copyable SocketT
             * 
             */
            { Backend::INVALID_SOCKET } -> std::convertible_to<typename Backend::SocketT>;

            //this method can be necessary for SocketT types that are non-copyable but movable. 
            //Using INVALID_SOCKET to instantiate methods will make a copy of it, which 
            //is what this method avoids. It (should) always return a fresh invalidSocket for non-copyable types.
            //for backends whose SocketT is trivial, this method can just "return INVALID_SOCKET". 
            { Backend::invalidSocket() } -> std::same_as<typename Backend::SocketT>;
        }
        && requires(const typename Backend::SocketT& socket,
                    Domain domain,
                    Service service,
                    Protocol protocol,
                    ShutdownPolicy what,
                    int backlog,
                    const typename Backend::AddressStorageT* addr,
                    typename Backend::AddressLenT len,
                    const typename Backend::ErrorT& err,
                    const char* sendBuff,
                    char* recvBuff,
                    size_t n) {
            #if NINTTP_SOCKET_BACKEND_REQUIRES_INIT == 1
            { Backend::init() } noexcept -> std::convertible_to<bool>;
            { Backend::deinit() } noexcept -> std::convertible_to<bool>;
            #endif
            { Backend::openSocket(domain, service, protocol) } noexcept -> std::same_as<typename Backend::SocketT>;
            { Backend::closeSocket(socket) } noexcept -> std::convertible_to<bool>;
            { Backend::isValidSocket(socket) } noexcept -> std::convertible_to<bool>;
            { Backend::shutdownSocket(socket, what) } noexcept -> std::convertible_to<bool>;

            //to handle endpoints: conceptually, ListenerSocket::bind takes the actual EndpointT that has a concept. That concept forces to convert to a generic 
            //AddressStorage *. That AddressStorage is what this backend functions take.
            //each backend then must implement a way to convert from that generic AddressStorage to the sockaddr* that the native API takes
            { Backend::bind(socket, addr, len) } noexcept -> std::convertible_to<bool>;
            { Backend::listen(socket, backlog) } noexcept -> std::convertible_to<bool>;
            { Backend::accept(socket) } noexcept -> std::same_as<std::optional<typename Backend::AddressBundleT>>;

            { Backend::connect(socket, addr, len) } noexcept -> std::convertible_to<bool>;

            { Backend::send(socket, sendBuff, n) } noexcept -> std::convertible_to<ssize_t>;
            { Backend::receive(socket, recvBuff, n) } noexcept -> std::convertible_to<ssize_t>;

            { Backend::getLastError() } noexcept -> std::same_as<typename Backend::ErrorT>;
            { Backend::getMsgFromError(err) } -> std::same_as<std::string>;
        };

} // namespace ninttp::internal