#pragma once

#include <concepts>
#include <cstddef>
#include <expected>
#include <type_traits>
#include <optional>
#include <span>
#include <string>

#include "../../../endpoints.hpp"
#include "../../types.hpp"

namespace ninttp::internal {
    enum class SocketCloseDisposition {
        Released,
        Retry,
        Unspecified
    };

    template<typename ErrorT>
    struct SocketCloseStatus {
        SocketCloseDisposition disposition;
        std::optional<ErrorT> error;
    };

    /**
     * @brief POSIX/Winsock-shaped socket backend contract.
     *
     * ninttp intentionally targets two backend families: POSIX sockets and
     * Winsock. The backend abstraction exists to hide those platform differences,
     * not to model every possible networking stack.
     *
     * Keep this concept structural and boring. Semantic requirements such as
     * ownership transfer, blocking behavior, and error capture are documented in
     * docs/socket-contracts.md because concepts cannot validate them honestly.
     */
    template <typename Backend>
    concept SocketBackend =
        requires {
            typename Backend::SocketT;
            typename Backend::ErrorT;
            typename Backend::AddressStorageT;
            typename Backend::AddressLenT;
            typename Backend::AddressBundleT;
            typename Backend::CloseStatusT;
        }
        && requires(typename Backend::AddressBundleT bundle) {
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
        && requires {
            { Backend::invalidSocket() } -> std::same_as<typename Backend::SocketT>;
        }
        && requires(const typename Backend::SocketT& socket,
                    Domain domain,
                    Service service,
                    Protocol protocol,
                    ShutdownPolicy what,
                    int backlog,
                    const typename Backend::AddressStorageT* addr,
                    typename Backend::AddressLenT addrLen,
                    typename Backend::ErrorT err,
                    std::span<const char> sendBuffer,
                    std::span<char> receiveBuffer) {
            #if NINTTP_SOCKET_BACKEND_REQUIRES_INIT == 1
            { Backend::init() } noexcept -> std::same_as<std::expected<void, typename Backend::ErrorT>>;
            { Backend::deinit() } noexcept -> std::same_as<std::expected<void, typename Backend::ErrorT>>;
            #endif

            { Backend::openSocket(domain, service, protocol) } noexcept
                -> std::same_as<std::expected<typename Backend::SocketT, typename Backend::ErrorT>>;
            { Backend::closeSocket(socket) } noexcept -> std::same_as<typename Backend::CloseStatusT>;
            { Backend::isUsableSocket(socket) } noexcept -> std::convertible_to<bool>;
            { Backend::shutdownSocket(socket, what) } noexcept
                -> std::same_as<std::expected<void, typename Backend::ErrorT>>;

            { Backend::bind(socket, addr, addrLen) } noexcept
                -> std::same_as<std::expected<void, typename Backend::ErrorT>>;
            { Backend::listen(socket, backlog) } noexcept
                -> std::same_as<std::expected<void, typename Backend::ErrorT>>;
            { Backend::accept(socket) } noexcept
                -> std::same_as<std::expected<typename Backend::AddressBundleT, typename Backend::ErrorT>>;
            { Backend::connect(socket, addr, addrLen) } noexcept
                -> std::same_as<std::expected<void, typename Backend::ErrorT>>;

            { Backend::toStorage(ninttp::IPv4Endpoint{}) } noexcept -> std::same_as<typename Backend::AddressStorageT>;
            { Backend::storageLen(ninttp::IPv4Endpoint{}) } noexcept -> std::convertible_to<typename Backend::AddressLenT>;
            { Backend::template fromStorage<ninttp::IPv4Endpoint>(*addr) } noexcept
                -> std::same_as<std::expected<ninttp::IPv4Endpoint, typename Backend::ErrorT>>;

            { Backend::send(socket, sendBuffer) } noexcept
                -> std::same_as<std::expected<std::size_t, typename Backend::ErrorT>>;
            { Backend::receive(socket, receiveBuffer) } noexcept
                -> std::same_as<std::expected<std::size_t, typename Backend::ErrorT>>;

            { Backend::getMsgFromError(err) } -> std::same_as<std::string>;
        };

} // namespace ninttp::internal
