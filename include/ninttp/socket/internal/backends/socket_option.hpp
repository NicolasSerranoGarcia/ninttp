/**
 * @file socket_option.hpp
 * @author Nicolas Serrano (serranogarcianicolas@gmail.com)
 * @brief Defines backend socket options exposed through the socket facade.
 * @version 0.1
 * @date 2026-07-02
 *
 * @copyright Copyright (c) 2026 Nicolas Serrano Garcia
 *
 */

#pragma once

namespace ninttp::internal {
    /**
     * @brief Backend socket options exposed through the socket facade.
     */
    enum class SocketOption {
        IPv6Only
    };
} // namespace ninttp::internal
