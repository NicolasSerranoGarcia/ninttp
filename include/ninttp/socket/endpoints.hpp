#pragma once

#include <cstdint>

namespace ninttp
{
    class Ipv4Endpoint{
        public:
            /**
             * @brief Builds the IPv4 wildcard endpoint on port 0.
             *
             * @pre None.
             * @post addressHostOrder() == 0.
             * @post portHostOrder() == 0.
             */
            constexpr Ipv4Endpoint() noexcept = default;

            /**
             * @brief Builds an IPv4 endpoint value.
             *
             * @pre hostOrderAddress is encoded as 0xAABBCCDD for address
             *      AA.BB.CC.DD. For example, loopback is 0x7F000001.
             * @pre hostOrderPort is expressed in host byte order.
             * @post addressHostOrder() == hostOrderAddress.
             * @post portHostOrder() == hostOrderPort.
             */
            constexpr Ipv4Endpoint(uint32_t hostOrderAddress, uint16_t hostOrderPort) noexcept
                : address_(hostOrderAddress), port_(hostOrderPort){}

            /**
             * @brief Returns the IPv4 address in host byte order.
             *
             * @pre None.
             * @post The returned value is encoded as 0xAABBCCDD for address
             *       AA.BB.CC.DD.
             */
            constexpr uint32_t addressHostOrder() const noexcept{
                return address_;
            }

            /**
             * @brief Returns the port in host byte order.
             *
             * @pre None.
             * @post The returned value is in host byte order.
             */
            constexpr uint16_t portHostOrder() const noexcept{
                return port_;
            }

        private:
            uint32_t address_ = 0;
            uint16_t port_ = 0;
    };
} // namespace ninttp
