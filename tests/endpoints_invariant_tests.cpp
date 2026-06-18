#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>

#include <ninttp/endpoints.hpp>
#include <ninttp/socket/internal/select_backend.hpp>
#include <ninttp/socket/utils.hpp>

namespace {

[[nodiscard]] bool checkEqual(const char* name, std::uint32_t actual, std::uint32_t expected) {
    if (actual == expected) {
        return true;
    }

    std::cerr << name << " failed: expected " << expected << ", got " << actual << '\n';
    return false;
}

[[nodiscard]] sockaddr_in readIpv4(const ninttp::internal::SelectedBackend::AddressStorageT& storage) {
    sockaddr_in out{};
    std::memcpy(&out, &storage, sizeof(out));
    return out;
}

} // namespace

int main() {
    bool ok = true;

    {
        constexpr std::uint32_t hostAddress = 0x7F000001u;
        constexpr std::uint16_t hostPort = 8080u;

        const ninttp::IPv4Endpoint endpoint(hostAddress, hostPort);
        const auto storage = ninttp::internal::SelectedBackend::toStorage(endpoint);
        const auto native = readIpv4(storage);

        ok = checkEqual("IPv4 host address", endpoint.addressHostOrder(), hostAddress) && ok;
        ok = checkEqual("IPv4 host port", endpoint.portHostOrder(), hostPort) && ok;
        ok = checkEqual("IPv4 family", native.sin_family, AF_INET) && ok;
        ok = checkEqual("IPv4 network address", native.sin_addr.s_addr, ninttp::hostToNetwork32(hostAddress)) && ok;
        ok = checkEqual("IPv4 network port", native.sin_port, ninttp::hostToNetwork16(hostPort)) && ok;
        ok = checkEqual("IPv4 storage length", ninttp::internal::SelectedBackend::storageLen(endpoint), sizeof(sockaddr_in)) && ok;
    }

    {
        ninttp::internal::SelectedBackend::AddressStorageT storage{};
        auto native = readIpv4(storage);
        native.sin_family = AF_INET;
        native.sin_addr.s_addr = ninttp::hostToNetwork32(0x0A000001u);
        native.sin_port = ninttp::hostToNetwork16(80u);
        std::memcpy(&storage, &native, sizeof(native));

        const auto endpoint = ninttp::internal::SelectedBackend::fromStorage<ninttp::IPv4Endpoint>(storage);
        const auto roundTrip = readIpv4(ninttp::internal::SelectedBackend::toStorage(endpoint));

        ok = checkEqual("IPv4 fromStorage family", roundTrip.sin_family, AF_INET) && ok;
        ok = checkEqual("IPv4 fromStorage address", roundTrip.sin_addr.s_addr, native.sin_addr.s_addr) && ok;
        ok = checkEqual("IPv4 fromStorage port", roundTrip.sin_port, native.sin_port) && ok;
        ok = checkEqual("IPv4 fromStorage host address", endpoint.addressHostOrder(), 0x0A000001u) && ok;
        ok = checkEqual("IPv4 fromStorage host port", endpoint.portHostOrder(), 80u) && ok;
    }

    {
        ninttp::internal::SelectedBackend::AddressStorageT storage{};
        bool threw = false;

        try {
            (void)ninttp::internal::SelectedBackend::fromStorage<ninttp::IPv4Endpoint>(storage);
        } catch (const std::runtime_error&) {
            threw = true;
        }

        if (!threw) {
            std::cerr << "IPv4 fromStorage rejected invalid family failed\n";
            ok = false;
        }
    }

    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
