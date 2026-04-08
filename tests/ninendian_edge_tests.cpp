#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>

#include <ninttp/ninendian.hpp>

namespace {

template <typename T>
[[nodiscard]] bool checkRoundTrip(const char* name, T value) {
    const T network = sizeof(T) == 2
        ? static_cast<T>(hostToNetwork16(static_cast<std::uint16_t>(value)))
        : static_cast<T>(hostToNetwork32(static_cast<std::uint32_t>(value)));

    const T host = sizeof(T) == 2
        ? static_cast<T>(networkToHost16(static_cast<std::uint16_t>(network)))
        : static_cast<T>(networkToHost32(static_cast<std::uint32_t>(network)));

    if (host == value) {
        return true;
    }

    std::cerr << name << " roundtrip failed for value " << static_cast<std::uint64_t>(value)
              << " (got " << static_cast<std::uint64_t>(host) << ")\n";
    return false;
}

[[nodiscard]] bool checkEqual(const char* name, std::uint32_t actual, std::uint32_t expected) {
    if (actual == expected) {
        return true;
    }

    std::cerr << name << " failed: expected " << expected << ", got " << actual << '\n';
    return false;
}

} // namespace

int main() {
    bool ok = true;

    constexpr std::array<std::uint16_t, 6> values16{
        0x0000u,
        0x0001u,
        0x00FFu,
        0x7FFFu,
        0x8000u,
        0xFFFFu,
    };

    constexpr std::array<std::uint32_t, 8> values32{
        0x00000000u,
        0x00000001u,
        0x000000FFu,
        0x0000FFFFu,
        0x7FFFFFFFu,
        0x80000000u,
        0xFF000000u,
        0xFFFFFFFFu,
    };

    for (const auto value : values16) {
        ok = checkRoundTrip("uint16_t", value) && ok;
    }

    for (const auto value : values32) {
        ok = checkRoundTrip("uint32_t", value) && ok;
    }

    ok = checkEqual("zero 16", hostToNetwork16(0x0000u), 0x0000u) && ok;
    ok = checkEqual("all ones 16", hostToNetwork16(0xFFFFu), 0xFFFFu) && ok;
    ok = checkEqual("zero 32", hostToNetwork32(0x00000000u), 0x00000000u) && ok;
    ok = checkEqual("all ones 32", hostToNetwork32(0xFFFFFFFFu), 0xFFFFFFFFu) && ok;

    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
