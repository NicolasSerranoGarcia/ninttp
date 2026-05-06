#pragma once

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

    /**
     * @brief The policy to use when shutting down (parts of) a socket
     * 
     * @see toNativeShutdownPolicy
     */
    enum class ShutdownPolicy{
        SHUT_RECEPTIONS, //! Shuts down receptions. This means that the socket that you call this policy on won't be able to receive further information from it's peer
        SHUT_TRANSMISSIONS, //! Shuts down transmissions. This means that the socket that you call this policy on won't be able to send further information to it's peer
        SHUT_TRANSMISSIONS_AND_RECEPTIONS //! Does the same as the other two combined
    };

} // namespace ninttp