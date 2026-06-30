/**
 * @file socket_error.hpp
 * @author Nicolás Serrano (serranogarcianicolas@gmail.com)
 * @brief Defines SocketError used on any socket operation
 * @version 0.1
 * @date 2026-04-08
 * 
 * @copyright Copyright (c) 2026 Nicolás Serrano García
 * 
 */

#pragma once

#include <array>
#include <cstdio>
#include <source_location>
#include <string>
#include <string_view>

#include "internal/select_backend.hpp"
#include "socket_error_category.hpp"

namespace ninttp
{
    /**
     * @brief Represents an error in a socket operation
     */
    class SocketError{
        public:
        using ErrorT = internal::SelectedBackend::ErrorT;
        /**
         * @brief Builds a SocketError instance from a native backend error.
         *
         * @param err Native backend error code.
         * @param location Source location of the caller.
         * @details source_location is not specified to be constexpr nor consteval so we can't mark this constructor as such.
         */
        SocketError(const ErrorT& err, const std::source_location& location = std::source_location::current()) noexcept
            : err_(err), category_(internal::SelectedBackend::categoryFromError(err))
        {
            setMessage(err, location);
        }

        /**
         * @brief Returns the preformatted error message.
         *
         * @details The message is prepared during construction, so this accessor does not
         * allocate and is safe to call from cleanup/error paths.
         */
        [[nodiscard]] const char* what() const noexcept{
            return message_.data();
        }

        /**
         * @brief Returns the backend native error code.
         */
        [[nodiscard]] ErrorT errorCode() const noexcept{
            return err_;
        }

        [[nodiscard]] SocketErrorCategory category() const noexcept{
            return category_;
        }

        private:
        static constexpr std::size_t MAX_MESSAGE_SIZE = 512;

        void copyMessage(std::string_view message) noexcept{
            const std::size_t count = message.size() < MAX_MESSAGE_SIZE ? message.size() : MAX_MESSAGE_SIZE - 1;
            for(std::size_t i = 0; i < count; ++i)
                message_[i] = message[i];
            message_[count] = '\0';
        }

        void setFallbackMessage(const ErrorT& originalErr, const std::source_location& location) noexcept{
            err_ = internal::SelectedBackend::insufficientMemoryError();
            category_ = internal::SelectedBackend::categoryFromError(err_);

            std::snprintf(
                message_.data(),
                message_.size(),
                "%s: error reporting of native socket error %d failed due to insufficient memory",
                location.function_name(),
                static_cast<int>(originalErr));
        }

        void setMessage(const ErrorT& err, const std::source_location& location) noexcept{
            try{
                std::string out{location.function_name()};
                out += ": ";
                out += internal::SelectedBackend::getMsgFromError(err);
                copyMessage(out);
            } catch(const std::bad_alloc&){
                setFallbackMessage(err, location);
            }
        }

        ErrorT err_; //! the native error code
        SocketErrorCategory category_;
        std::array<char, MAX_MESSAGE_SIZE> message_{};
    };
} // namespace ninttp
