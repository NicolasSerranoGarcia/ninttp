/**
 * @file concepts.hpp
 * @author Nicolás Serrano (serranogarcianicolas@gmail.com)
 * @brief Socket facade concepts and compile-time helpers.
 * @version 0.1
 * @date 2026-07-02
 *
 * @copyright Copyright (c) 2026 Nicolás Serrano García
 *
 */

#pragma once

#include <concepts>

#include "internal/select_backend.hpp"
#include "traits.hpp"

namespace ninttp::concepts
{
    /**
     * @brief Compile-time socket domain associated with a concrete endpoint type.
     */
    template<typename EndpointT>
    inline constexpr Domain endpointDomain =
        std::same_as<EndpointT, IPv4Endpoint> ? Domain::IPv4 : Domain::IPv6;

    /**
     * @brief Checks that a connected socket can adopt a native accepted socket.
     */
    template<typename ConnectedSocketT, typename EndpointT>
    concept AcceptedConnectedSocket = requires(
        typename internal::SelectedBackend::SocketT socket,
        Domain domain,
        Service service,
        Protocol protocol,
        const EndpointT& endpoint) {
            { ConnectedSocketT(socket, domain, service, protocol, endpoint) } noexcept
                -> std::same_as<ConnectedSocketT>;
        };
} // namespace ninttp::concepts
