#include <ninttp/dns/dns.hpp>
#include <iostream>
#include <string>

int main() {
    // These catch the old constant-loopback result and incorrect byte order/port.
    auto numeric = ninttp::dns::resolve("192.0.2.42", 4321);
    if (!numeric || numeric->ipv4.size() != 1 || !numeric->ipv6.empty() || numeric->ipv4.front().addressHostOrder() != 0xc000022au || numeric->ipv4.front().portHostOrder() != 4321)
        return 1;
    const std::string backing = "127.0.0.1ignored";
    auto sliced = ninttp::dns::resolve(std::string_view{backing.data(), 9});
    if (!sliced || sliced->ipv4.empty() || sliced->ipv4.front().addressHostOrder() != 0x7f000001u || sliced->ipv4.front().portHostOrder() != 0)
        return 2;
    auto local = ninttp::dns::resolve(std::string{"localhost"}, 80);
    if (!local || (local->ipv4.empty() && local->ipv6.empty()))
        return 3;
    for (const auto& endpoint : local->ipv4)
        if (endpoint.portHostOrder() != 80) return 6;
    for (const auto& endpoint : local->ipv6)
        if (endpoint.portHostOrder() != 80) return 7;
    if (ninttp::dns::resolve("") || ninttp::dns::resolve(std::string_view{"localhost\0ignored", 17}))
        return 4;
    // Exercise all sixteen address bytes without depending on external DNS.
    auto ipv6 = ninttp::dns::resolve("2001:db8::1234", 8443);
    const ninttp::IPv6Endpoint::AddressBytes expected{
        0x20, 0x01, 0x0d, 0xb8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x12, 0x34};
    if (!ipv6 || !ipv6->ipv4.empty() || ipv6->ipv6.size() != 1
        || ipv6->ipv6.front().addressBytes() != expected
        || ipv6->ipv6.front().portHostOrder() != 8443)
        return 5;
    std::cout << "DNS resolution tests passed\n";
}
