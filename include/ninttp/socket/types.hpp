/**
 * @file types.hpp
 * @author Nicolas Serrano (serranogarcianicolas@gmail.com)
 * @brief Defines common socket domain, service, protocol, and shutdown policy types.
 * @version 0.1
 * @date 2026-06-26
 *
 * @copyright Copyright (c) 2026 Nicolas Serrano Garcia
 *
 */

#pragma once

namespace ninttp
{
    /**
     * @brief Address family used when opening a socket.
     */
    enum class Domain{
        Invalid = -1,
        IPv4,
        IPv6,
        Unix,
    };

    /**
     * @brief Communication semantics requested from the native socket backend.
     */
    enum class Service{
        Invalid = -1,
        Stream,
        Datagram,
        Raw,
        SeqPacket,
    };

    /**
     * @brief Transport protocol requested from the native socket backend.
     */
    enum class Protocol{
        Invalid = -1,
        Default,
        Tcp,
        Udp,
    };

    /**
     * @brief The policy to use when shutting down (parts of) a socket
     * 
     * @see toNativeShutdownPolicy
     */
    enum class ShutdownPolicy{
        SHUT_RECEPTIONS, //!< Disables further receive operations on the socket.
        SHUT_TRANSMISSIONS, //!< Disables further send operations on the socket.
        SHUT_TRANSMISSIONS_AND_RECEPTIONS //!< Disables both receive and send operations.
    };

} // namespace ninttp
