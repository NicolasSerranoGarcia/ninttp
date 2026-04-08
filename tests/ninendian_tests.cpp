#include <cstdint>
#include <cstdlib>
#include <iostream>

#include <ninttp/ninendian.hpp>

namespace {

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

    const std::uint16_t v16 = 0x1234u;
    const std::uint32_t v32 = 0x12345678u;

    const std::uint16_t network16 = hostToNetwork16(v16);
    const std::uint32_t network32 = hostToNetwork32(v32);

#if NINTTP_BYTE_ORDER == NINTTP_LITTLE_ENDIAN
    ok = checkEqual("hostToNetwork16", network16, 0x3412u) && ok;
    ok = checkEqual("hostToNetwork32", network32, 0x78563412u) && ok;
#elif NINTTP_BYTE_ORDER == NINTTP_BIG_ENDIAN
    ok = checkEqual("hostToNetwork16", network16, 0x1234u) && ok;
    ok = checkEqual("hostToNetwork32", network32, 0x12345678u) && ok;
#else
    std::cerr << "NINTTP_BYTE_ORDER is not defined to a supported value\n";
    ok = false;
#endif

    ok = checkEqual("networkToHost16 roundtrip", networkToHost16(network16), v16) && ok;
    ok = checkEqual("networkToHost32 roundtrip", networkToHost32(network32), v32) && ok;

    ok = checkEqual("16-bit involution", hostToNetwork16(network16), v16) && ok;
    ok = checkEqual("32-bit involution", hostToNetwork32(network32), v32) && ok;

    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
