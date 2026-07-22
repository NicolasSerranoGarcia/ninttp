/**
 * @file utils.hpp
 * @author Nicolas Serrano (serranogarcianicolas@gmail.com)
 * @brief Defines socket byte-order conversion helpers
 * @version 0.1
 * @date 2026-06-26
 *
 * @copyright Copyright (c) 2026 Nicolas Serrano Garcia
 *
 */

#pragma once

#include <cstdint>

#include "../platform_traits.hpp"

namespace ninttp::utils
{
    /**
     * @brief Converts a 16-bit integer from host byte order to network byte order.
     *
     * @param value Value encoded in the current host byte order.
     * @return Value encoded in network byte order.
     */
    [[nodiscard]] inline constexpr std::uint16_t hostToNetwork16(std::uint16_t value) noexcept{
        #if NINTTP_BYTE_ORDER == NINTTP_LITTLE_ENDIAN
            return static_cast<std::uint16_t>((value >> 8) | (value << 8));
        #else
            return value;
        #endif
    }

    /**
     * @brief Converts a 16-bit integer from network byte order to host byte order.
     *
     * @param value Value encoded in network byte order.
     * @return Value encoded in the current host byte order.
     */
    [[nodiscard]] inline constexpr std::uint16_t networkToHost16(std::uint16_t value) noexcept{
        #if NINTTP_BYTE_ORDER == NINTTP_LITTLE_ENDIAN
            return static_cast<std::uint16_t>((value >> 8) | (value << 8));
        #else
            return value;
        #endif
    }

    /**
     * @brief Converts a 32-bit integer from host byte order to network byte order.
     *
     * @param value Value encoded in the current host byte order.
     * @return Value encoded in network byte order.
     */
    [[nodiscard]] inline constexpr std::uint32_t hostToNetwork32(std::uint32_t value) noexcept{
        #if NINTTP_BYTE_ORDER == NINTTP_LITTLE_ENDIAN
            return static_cast<std::uint32_t>(((value & 0x000000FFu) << 24) |
                ((value & 0x0000FF00u) << 8) |
                ((value & 0x00FF0000u) >> 8) |
                ((value & 0xFF000000u) >> 24));
        #else
            return value;
        #endif
    }

    /**
     * @brief Converts a 32-bit integer from network byte order to host byte order.
     *
     * @param value Value encoded in network byte order.
     * @return Value encoded in the current host byte order.
     */
    [[nodiscard]] inline constexpr std::uint32_t networkToHost32(std::uint32_t value) noexcept{
        #if NINTTP_BYTE_ORDER == NINTTP_LITTLE_ENDIAN
            return static_cast<std::uint32_t>(((value & 0x000000FFu) << 24) |
                ((value & 0x0000FF00u) << 8) |
                ((value & 0x00FF0000u) >> 8) |
                ((value & 0xFF000000u) >> 24));
        #else
            return value;
        #endif
    }
} // namespace ninttp::utils
