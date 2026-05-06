/**
 * @file socket_error.hpp
 * @author Nicolás Serrano (serranogarcianicolas@gmail.com)
 * @brief Defines socketError used on any socket operation
 * @version 0.1
 * @date 2026-04-08
 * 
 * @copyright Copyright (c) 2026 Nicolás Serrano García
 * 
 */

#pragma once

#include <source_location>
#include <string>

#include "internal/select_backend.hpp"

namespace ninttp
{
    /**
     * @brief Represents an error in a socket operation
     */
    struct socketError{
        using ErrorT = internal::SelectedBackend::ErrorT;
        /**
         * @brief Builds a socketError instance from an optional context and the location of the caller
         * @param context Optional context string
         * @param location Source location of the caller
         * @note it is safe to pass @c nullptr to @p context
         * @details source_location is not specified to be constexpr nor consteval so we can't mark this constructor as such
         */
        socketError(const ErrorT& err, const std::source_location& location = std::source_location::current()) noexcept
            : err_(err), location_(location){}

        /**
         * @brief Returns a comprehensive error message combining the location and the context given in construction
         * @return The message composed from the call-site function and the optional context
         * @details SocketError::msg() can be verbose. If you prefer to handle the error yourself, you may want to consult the nativeError_ code.
         * On unix platforms comes from errno, for example.
         */
        std::string msg() const{
            std::string out{location_.function_name()};
            out += ": ";
            out += internal::SelectedBackend::getMsgFromError(err_);
            return out;
        }

        std::string context() const{
            return internal::SelectedBackend::getMsgFromError(err_);
        }

        internal::SelectedBackend::ErrorT err_; //! the native error code
        std::source_location location_; //! the source location of the caller
    };
} // namespace ninttp
