/**
 * @file socket_close_status.hpp
 * @author Nicolas Serrano (serranogarcianicolas@gmail.com)
 * @brief Defines backend socket close ownership status.
 * @version 0.1
 * @date 2026-07-02
 *
 * @copyright Copyright (c) 2026 Nicolas Serrano Garcia
 *
 */

#pragma once

#include <optional>

namespace ninttp::internal {
    /**
     * @brief Describes whether a failed close released ownership of the native socket.
     */
    enum class SocketCloseDisposition {
        Released,
        Retry,
        Unspecified
    };

    /**
     * @brief Close result containing both ownership disposition and the native error.
     */
    template<typename ErrorT>
    struct SocketCloseStatus {
        SocketCloseDisposition disposition;
        std::optional<ErrorT> error;
    };
} // namespace ninttp::internal
