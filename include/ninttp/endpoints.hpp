/**
 * @file endpoints.hpp
 * @author Nicolás Serrano (serranogarcianicolas@gmail.com)
 * @brief Defines IPv4 and IPv6 enpoints to use with the ninttp interface
 * @version 0.2
 * @date 2026-07-02
 * 
 * @copyright Copyright (c) 2026 Nicolás Serrano García
 * 
 */

#pragma once

#include <array>
#include <cstdint>

namespace ninttp
{
    class IPv4Endpoint{
        public:
            /**
             * @brief Builds the IPv4 wildcard endpoint on port 0.
             *
             * @pre None.
             * @post addressHostOrder() == 0.
             * @post portHostOrder() == 0.
             */
            constexpr IPv4Endpoint() noexcept = default;

            /**
             * @brief Builds an IPv4 endpoint value.
             *
             * @pre hostOrderAddress is encoded as 0xAABBCCDD for address
             *      AA.BB.CC.DD. For example, loopback is 0x7F000001.
             * @pre hostOrderPort is expressed in host byte order.
             * @post addressHostOrder() == hostOrderAddress.
             * @post portHostOrder() == hostOrderPort.
             */
            constexpr IPv4Endpoint(std::uint32_t hostOrderAddress, std::uint16_t hostOrderPort) noexcept
                : address_(hostOrderAddress), port_(hostOrderPort){}

            /**
             * @brief Returns the IPv4 address in host byte order.
             *
             * @pre None.
             * @post The returned value is encoded as 0xAABBCCDD for address
             *       AA.BB.CC.DD.
             */
            [[nodiscard]] constexpr std::uint32_t addressHostOrder() const noexcept{
                return address_;
            }

            /**
             * @brief Returns the port in host byte order.
             *
             * @pre None.
             * @post The returned value is in host byte order.
             */
            [[nodiscard]] constexpr std::uint16_t portHostOrder() const noexcept{
                return port_;
            }

            [[nodiscard]] static constexpr inline IPv4Endpoint loopback(std::uint16_t hostOrderPort) noexcept{
                return IPv4Endpoint{0x7F000001u, hostOrderPort};
            }

        private:
            std::uint32_t address_ = 0;
            std::uint16_t port_ = 0;
    };

    class IPv6Endpoint{
        public:
            using AddressBytes = std::array<std::uint8_t, 16>;

            /**
             * @brief Builds the IPv6 unspecified endpoint on port 0.
             *
             * @pre None.
             * @post addressBytes() contains sixteen zero bytes.
             * @post portHostOrder() == 0.
             */
            constexpr IPv6Endpoint() noexcept = default;

            /**
             * @brief Builds an IPv6 endpoint value.
             *
             * @pre addressBytes is encoded in network byte order, from the first IPv6 address
             *      octet to the last one.
             * @pre hostOrderPort is expressed in host byte order.
             * @post addressBytes() == addressBytes.
             * @post portHostOrder() == hostOrderPort.
             */
            constexpr IPv6Endpoint(AddressBytes addressBytes, std::uint16_t hostOrderPort) noexcept
                : address_(addressBytes), port_(hostOrderPort){}

            /**
             * @brief Returns the IPv6 address bytes in network byte order.
             *
             * @pre None.
             * @post The returned value contains sixteen bytes, from the first IPv6 address octet
             *       to the last one.
             */
            [[nodiscard]] constexpr AddressBytes addressBytes() const noexcept{
                return address_;
            }

            /**
             * @brief Returns the port in host byte order.
             *
             * @pre None.
             * @post The returned value is in host byte order.
             */
            [[nodiscard]] constexpr std::uint16_t portHostOrder() const noexcept{
                return port_;
            }

            [[nodiscard]] static constexpr inline IPv6Endpoint loopback(std::uint16_t hostOrderPort) noexcept{
                AddressBytes bytes{};
                bytes[15] = 1;
                return IPv6Endpoint{bytes, hostOrderPort};
            }

        private:
            AddressBytes address_{};
            std::uint16_t port_ = 0;
    };
} // namespace ninttp
