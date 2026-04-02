#pragma once

#include <array>
#include <string>
#include <cstddef>
#include <cstdint>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdexcept>

namespace ninttp
{
    //Endpoint interface recap. must be convertible to sockaddr*, must have a .size()
    //must be constructible from the info that ::accept() returns

    //ConnectedSocketT recap. must be constructible from what ::accept() returns overall

    //enforce this not on the created endpoint classes but rather on the callee with concepts?

    class IEndpoint{

    };

    //TODO: refactor endpoints with the specifications

    //supports compressed IPv6 text through inet_pton
    struct Ipv6Endpoint{
        std::array<uint16_t, 8> hextet{};

        constexpr bool empty() const noexcept
        {
            for (const uint16_t value : hextet) {
                if (value != 0) {
                    return false;
                }
            }

            return true;
        }

        constexpr Ipv6Endpoint() = default;

        constexpr Ipv6Endpoint(const std::array<uint16_t, 8>& value) noexcept
            : hextet(value){}

        Ipv6Endpoint(const in6_addr& value) noexcept
        {
            for (std::size_t i = 0; i < hextet.size(); ++i) {
                const uint16_t native = static_cast<uint16_t>(
                    (static_cast<uint16_t>(value.s6_addr[i * 2]) << 8) |
                    static_cast<uint16_t>(value.s6_addr[i * 2 + 1])
                );

                hextet[i] = ntohs(native);
            }
        }

        Ipv6Endpoint(const std::string& value) noexcept
        {
            in6_addr native{};
            if (::inet_pton(AF_INET6, value.c_str(), &native) == 1) {
                *this = Ipv6Endpoint(native);
            }
        }

        Ipv6Endpoint(const char* value) noexcept
        {
            if (value == nullptr) {
                return;
            }

            in6_addr native{};
            if (::inet_pton(AF_INET6, value, &native) == 1) {
                *this = Ipv6Endpoint(native);
            }
        }

        operator std::array<uint16_t, 8>() const noexcept
        {
            return hextet;
        }

        operator in6_addr() const noexcept
        {
            in6_addr value{};

            for (std::size_t i = 0; i < hextet.size(); ++i) {
                const uint16_t network = htons(hextet[i]);
                value.s6_addr[i * 2] = static_cast<uint8_t>((network >> 8) & 0xFF);
                value.s6_addr[i * 2 + 1] = static_cast<uint8_t>(network & 0xFF);
            }

            return value;
        }
    };

    class Ipv4Endpoint{
        public:

            constexpr Ipv4Endpoint() noexcept{
                native_.sin_family = AF_INET;
            };

            Ipv4Endpoint(const in_addr& addr, uint16_t port) noexcept : Ipv4Endpoint(){
                setAddress(addr);
                setPort(port);
            }

            constexpr Ipv4Endpoint(uint32_t hostOrderAddress, uint16_t port) noexcept
                : native_{}
            {
                native_.sin_family = AF_INET;
                native_.sin_addr.s_addr = hostToNetwork32_(hostOrderAddress);
                native_.sin_port = hostToNetwork16_(port);
            }

            Ipv4Endpoint(const char* textAddress, uint16_t port) : Ipv4Endpoint(){
                setAddress(textAddress);
                setPort(port);
            }

            Ipv4Endpoint(const std::string& textAddress, uint16_t port) : Ipv4Endpoint(){
                setAddress(textAddress);
                setPort(port);
            }

            static constexpr Ipv4Endpoint fromOctets(
                uint8_t a,
                uint8_t b,
                uint8_t c,
                uint8_t d,
                uint16_t port = 0) noexcept
            {
                return Ipv4Endpoint(hostOrderFromOctets_(a, b, c, d), port);
            }

            Ipv4Endpoint(const sockaddr_storage& addr){
                if(addr.ss_family != AF_INET){
                    throw std::runtime_error("Invalid sockaddr_storage");
                }

                native_ = reinterpret_cast<const sockaddr_in&>(addr);
            }

            sockaddr_in toSockaddr() noexcept{
                return native_;
            }

            operator sockaddr*() noexcept{
                return reinterpret_cast<sockaddr*>(&native_);
            }

            operator const sockaddr*() const noexcept{
                return reinterpret_cast<const sockaddr*>(&native_);
            }

            void setAddress(const in_addr& addr) noexcept{
                native_.sin_addr = addr;
            }

            void setAddress(uint32_t hostOrderAddress) noexcept{
                native_.sin_addr.s_addr = htonl(hostOrderAddress);
            }

            void setAddress(const char* textAddress){
                if (textAddress == nullptr || ::inet_pton(AF_INET, textAddress, &native_.sin_addr) != 1) {
                    throw std::runtime_error("bad IPv4");
                }
            }

            void setAddress(const std::string& textAddress){
                setAddress(textAddress.c_str());
            }

            in_addr addressInAddr() const noexcept{
                return native_.sin_addr;
            }

            uint32_t addressHostOrder() const noexcept{
                return ntohl(native_.sin_addr.s_addr);
            }

            std::string addressString() const{
                char buf[INET_ADDRSTRLEN]{};
                if (::inet_ntop(AF_INET, &native_.sin_addr, buf, sizeof(buf)) == nullptr) {
                    return {};
                }

                return std::string(buf);
            }

            void setPort(uint16_t port) noexcept{
                native_.sin_port = htons(port);
            }

            void setPortNetworkOrder(uint16_t portNetworkOrder) noexcept{
                native_.sin_port = portNetworkOrder;
            }

            uint16_t port() const noexcept{
                return ntohs(native_.sin_port);
            }

            uint16_t portNetworkOrder() const noexcept{
                return native_.sin_port;
            }

            size_t size() const noexcept{
                return sizeof(sockaddr_in);
            };

            static Ipv4Endpoint fromSockAddr(const sockaddr_storage* addr){
                if(addr->ss_family != AF_INET){
                    throw std::runtime_error("Invalid sockaddr_storage");
                }

                const sockaddr_in* in = reinterpret_cast<const sockaddr_in*>(addr);

                Ipv4Endpoint out;
                out.native_ = *in;
                
                return out;
            }

        private:
            mutable sockaddr_in native_{};

            static constexpr uint16_t hostToNetwork16_(uint16_t value) noexcept{
                #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
                                return static_cast<uint16_t>((value >> 8) | (value << 8));
                #else
                                return value;
                #endif
            }

            static constexpr uint32_t hostToNetwork32_(uint32_t value) noexcept{
                #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
                                return ((value & 0x000000FFu) << 24) |
                                    ((value & 0x0000FF00u) << 8)  |
                                    ((value & 0x00FF0000u) >> 8)  |
                                    ((value & 0xFF000000u) >> 24);
                #else
                                return value;
                #endif
            }

            
            static constexpr uint32_t hostOrderFromOctets_(
                uint8_t a,
                uint8_t b,
                uint8_t c,
                uint8_t d) noexcept
            {
                return (static_cast<uint32_t>(a) << 24) |
                       (static_cast<uint32_t>(b) << 16) |
                       (static_cast<uint32_t>(c) << 8) |
                       (static_cast<uint32_t>(d));
            }
    };

    inline constexpr Ipv4Endpoint v4Addr_any = Ipv4Endpoint::fromOctets(0, 0, 0, 0, 0);
    inline constexpr Ipv4Endpoint v4Addr_loopback = Ipv4Endpoint::fromOctets(127, 0, 0, 1, 0);
    // inline constexpr Ipv6Endpoint v6Addr_any{std::array<uint16_t, 8>{0, 0, 0, 0, 0, 0, 0, 0}};
    // inline constexpr Ipv6Endpoint v6Addr_loopback{std::array<uint16_t, 8>{0, 0, 0, 0, 0, 0, 0, 1}};
} // namespace ninttp