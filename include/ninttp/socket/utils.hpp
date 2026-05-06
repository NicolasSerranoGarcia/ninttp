#pragma once

#include "../nintraits.hpp"

namespace ninttp
{
    inline constexpr uint16_t hostToNetwork16(uint16_t value) noexcept{
        #if NINTTP_BYTE_ORDER == NINTTP_LITTLE_ENDIAN
                        return static_cast<uint16_t>((value >> 8) | (value << 8));
        #else
                        return value;
        #endif
    }

    inline constexpr uint16_t networkToHost16(uint16_t value) noexcept{
        #if NINTTP_BYTE_ORDER == NINTTP_LITTLE_ENDIAN
                        return static_cast<uint16_t>((value >> 8) | (value << 8));
        #else
                        return value;
        #endif
    }

    //converts a host unsigned 32 integer to network endianness.
    //Note that value is assumed to have endianness equal to the system that this function is being used on.
    //passing any other endianness will yeld an undesired value
    inline constexpr uint32_t hostToNetwork32(uint32_t value) noexcept{
        #if NINTTP_BYTE_ORDER == NINTTP_LITTLE_ENDIAN
                        return static_cast<uint32_t>(((value & 0x000000FFu) << 24) |
                            ((value & 0x0000FF00u) << 8) |
                            ((value & 0x00FF0000u) >> 8) |
                            ((value & 0xFF000000u) >> 24));
        #else
                        return value;
        #endif
    }

    inline constexpr uint32_t networkToHost32(uint32_t value) noexcept{
        #if NINTTP_BYTE_ORDER == NINTTP_LITTLE_ENDIAN
                        return static_cast<uint32_t>(((value & 0x000000FFu) << 24) |
                            ((value & 0x0000FF00u) << 8) |
                            ((value & 0x00FF0000u) >> 8) |
                            ((value & 0xFF000000u) >> 24));
        #else
                        return value;
        #endif
    }
} // namespace ninttp
