#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include <arpa/inet.h>

#include <ninttp/socket/endpoints.hpp>

namespace {

[[nodiscard]] bool checkEqual(const char* name, std::uint32_t actual, std::uint32_t expected) {
    if (actual == expected) {
        return true;
    }

    std::cerr << name << " failed: expected " << expected << ", got " << actual << '\n';
    return false;
}

[[nodiscard]] bool checkBytesEqual(const char* name, const in6_addr& actual, const in6_addr& expected) {
    if (std::memcmp(actual.s6_addr, expected.s6_addr, sizeof(actual.s6_addr)) == 0) {
        return true;
    }

    std::cerr << name << " failed: IPv6 bytes mismatch\n";
    return false;
}

} // namespace

int main() {
    bool ok = true;

    {
        constexpr std::uint32_t hostAddress = 0x7F000001u;
        constexpr std::uint16_t hostPort = 8080u;

        ninttp::Ipv4Endpoint endpoint(hostAddress, hostPort);

        ok = checkEqual("IPv4 host getter", endpoint.addressHostOrder(), hostAddress) && ok;
        ok = checkEqual("IPv4 network getter", endpoint.addressNetworkOrder(), hostToNetwork32(hostAddress)) && ok;
        ok = checkEqual("IPv4 port host getter", endpoint.portHostOrder(), hostPort) && ok;
        ok = checkEqual("IPv4 port network getter", endpoint.portNetworkOrder(), hostToNetwork16(hostPort)) && ok;
    }

    {
        in_addr hostAddress{};
        hostAddress.s_addr = 0x0A000001u;

        ninttp::Ipv4Endpoint endpoint(hostAddress, 80u);
        ok = checkEqual("IPv4 in_addr host invariant", endpoint.addressHostOrder(), hostAddress.s_addr) && ok;

        in_addr networkAddress{};
        ::inet_pton(AF_INET, "127.0.0.1", &networkAddress);
        endpoint.setAddressNetworkOrder(networkAddress);

        ok = checkEqual("IPv4 set network order", endpoint.addressNetworkOrder(), networkAddress.s_addr) && ok;
        ok = checkEqual("IPv4 set network host read", endpoint.addressHostOrder(), networkToHost32(networkAddress.s_addr)) && ok;
    }

    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
