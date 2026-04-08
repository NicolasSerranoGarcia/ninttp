#pragma once

#include <cstdint>

//the name has a prefix to avoid naming issues with system includes

//defines endianness utilities at compile time

//defines NINTTP_BYTE_ORDER to be NINTTP_LITTLE_ENDIAN or NINTTP_BIG_ENDIAN
//depending on what is the host endianness

//Always defined when including ninendian.hpp
#define NINTTP_BIG_ENDIAN 4321
//Always defined when including ninendian.hpp
#define NINTTP_LITTLE_ENDIAN 1234

#if defined __unix__ || defined __unix
    #include <sys/param.h>
    #if defined __BYTE_ORDER
        #if __BYTE_ORDER == __LITTLE_ENDIAN
            #define NINTTP_BYTE_ORDER NINTTP_LITTLE_ENDIAN
        #elif __BYTE_ORDER == __BIG_ENDIAN
            #define NINTTP_BYTE_ORDER NINTTP_BIG_ENDIAN
        #endif
    #endif

    #if defined __BYTE_ORDER__
        #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
            #define NINTTP_BYTE_ORDER NINTTP_LITTLE_ENDIAN
        #elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
            #define NINTTP_BYTE_ORDER NINTTP_BIG_ENDIAN
        #endif
    #endif
#endif

#ifdef NINTTP_BYTE_ORDER
    #define NINTTP_BYTE_ORDER_DEFINED 1
#endif

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