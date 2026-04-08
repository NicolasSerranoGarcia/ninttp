#pragma once

#include <array>
#include <string>
#include <cstddef>
#include <cstdint>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdexcept>

#include "../ninendian.hpp"
//                                                                                                           0x0  0x1  0x2  0x3
//Network order is Big endian. This means that MSB is stored on lowest address. 0x12345678 would be saved as 12   34   56   78
//Little endian means 0x12345678 is stored as                                                                78   56   34   12

//abstract rule of thumb: Big endian is natural read order. Little endian is inverse read order (bytes, though, not bit by bit)

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

    //methods that take in addresses from unix and stuff do not have their parameters endian-checked.
    //this must have an internal invariant that it always saves the internal address state as either big endian, or host order, or network order...
    //and methods should not change that
    //right now this is highly coupled to Unix. For now just leave it like that
    //this class (and all endpoints) should ALWAYS take any user-supplied parameter as host order, and store it as native. Returns are then of host and network types
    //Constructors and setters ALWAYS take host order parameters
    class Ipv4Endpoint{
        public:

            constexpr Ipv4Endpoint() noexcept{
                native_.sin_family = AF_INET;
            };

            //takes in hostOrder
            constexpr Ipv4Endpoint(const in_addr& addr, uint16_t port) noexcept : Ipv4Endpoint(){
                setAddressHostOrder(addr);
                setPortHostOrder(port);
            }

            constexpr Ipv4Endpoint(uint32_t hostOrderAddress, uint16_t port) noexcept
                : native_{}
            {
                native_.sin_family = AF_INET;
                native_.sin_addr.s_addr = hostToNetwork32(hostOrderAddress);
                native_.sin_port = hostToNetwork16(port);
            }

            Ipv4Endpoint(const char* textAddress, uint16_t port) : Ipv4Endpoint(){
                setAddress(textAddress);
                setPortHostOrder(port);
            }

            Ipv4Endpoint(const std::string& textAddress, uint16_t port) : Ipv4Endpoint(){
                setAddress(textAddress);
                setPortHostOrder(port);
            }

            //e.g: a = 127, b = 0, c = 0, d = 1 would be loopback
            static constexpr Ipv4Endpoint fromOctets(
                uint8_t a,
                uint8_t b,
                uint8_t c,
                uint8_t d,
                uint16_t port = 0) noexcept
            {
                return Ipv4Endpoint(hostOrderFromOctets_(a, b, c, d), port);
            }

            //all of the endpoints should have this
            Ipv4Endpoint(const sockaddr_storage& addr){
                if(addr.ss_family != AF_INET){
                    throw std::runtime_error("Invalid sockaddr_storage");
                }

                native_ = reinterpret_cast<const sockaddr_in&>(addr);
            }

            constexpr sockaddr_in toSockaddr() noexcept{
                return native_;
            }

            operator sockaddr*() noexcept{
                return reinterpret_cast<sockaddr*>(&native_);
            }

            operator const sockaddr*() const noexcept{
                return reinterpret_cast<const sockaddr*>(&native_);
            }

            constexpr void setAddressHostOrder(const in_addr& addr) noexcept{
                native_.sin_addr.s_addr = hostToNetwork32(addr.s_addr);
            }

            constexpr void setAddressHostOrder(uint32_t addr) noexcept{
                native_.sin_addr.s_addr = hostToNetwork32(addr);
            }

            constexpr void setAddressNetworkOrder(const in_addr& addr) noexcept{
                native_.sin_addr = addr;
            }

            constexpr void setAddressNetworkOrder(uint32_t addr) noexcept{
                native_.sin_addr.s_addr = addr;
            }

            void setAddress(const char* textAddress){
                if (textAddress == nullptr || ::inet_pton(AF_INET, textAddress, &native_.sin_addr) != 1) {
                    throw std::runtime_error("bad IPv4");
                }
            }

            void setAddress(const std::string& textAddress){
                setAddress(textAddress.c_str());
            }

            constexpr in_addr addressHostInAddr() const noexcept{
                return in_addr{ .s_addr = networkToHost32(native_.sin_addr.s_addr) };
            }

            constexpr in_addr addressNetworkInAddr() const noexcept{
                return native_.sin_addr;
            }

            constexpr uint32_t addressNetworkOrder() const noexcept{
                return native_.sin_addr.s_addr;
            }

            constexpr uint32_t addressHostOrder() const noexcept{
                return networkToHost32(native_.sin_addr.s_addr);
            }

            std::string addressString() const{
                char buf[INET_ADDRSTRLEN]{};
                if (::inet_ntop(AF_INET, &native_.sin_addr, buf, sizeof(buf)) == nullptr) {
                    return {};
                }

                return std::string(buf);
            }

            //takes in the port with host order format
            constexpr void setPortHostOrder(uint16_t port) noexcept{
                native_.sin_port = hostToNetwork16(port);
            }

            //takes in the port with network order format
            constexpr void setPortNetworkOrder(uint16_t port) noexcept{
                native_.sin_port = port;
            }

            //returns the port in host order format
            constexpr uint16_t portHostOrder() const noexcept{
                return networkToHost16(native_.sin_port);
            }

            //returns port in network order format (big endian)
            constexpr uint16_t portNetworkOrder() const noexcept{
                return native_.sin_port;
            }

            //all endpoints must have this method
            //we might want to create a separate size bc this is not the intended result
            constexpr size_t size() const noexcept{
                return sizeof(sockaddr_in);
            };

            //throws so can't constexpr until c++26
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
            //this is always stored in network order
            sockaddr_in native_{};
            
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
} // namespace ninttp