/**
 * @file errors.hpp
 * @author Nicolás Serrano (serranogarcianicolas@gmail.com)
 * @brief Defines different error types used on sockets
 * @version 0.1
 * @date 2026-04-03
 * 
 * @copyright Copyright (c) 2026 Nicolás Serrano García
 * 
 */

#pragma once

#include <string>
#include <netinet/in.h>
#include <sys/un.h>

#include <source_location>

namespace ninttp
{
    /**
     * @brief Represents an error in a socket operation
     */
    struct socketError{
        /**
         * @brief Builds a socketError instance from an optional context and the location of the caller
         * @param context Optional context string
         * @param location Source location of the caller
         * @note it is safe to pass @c nullptr to @p context
         * @details source_location is not specified to be constexpr nor consteval so we can't mark this constructor as such
         */
        socketError(const char* context = nullptr, const std::source_location& location = std::source_location::current()) noexcept 
            : location_(location), context_(context == nullptr ? "" : context){}

        /**
         * @brief Returns a comprehensive error message combining the location and the context given in construction
         * @return The message composed from the call-site function and the optional context
         * @details SocketError::msg() can be verbose. If you prefer to handle the error yourself, you may want to consult the nativeError_ code.
         * On unix platforms comes from errno, for example.
         */
        constexpr std::string msg() const noexcept{
            return location_.function_name() + std::string(": ") + context_;
        }

        
        std::source_location location_; //! the source location of the caller
        std::string context_; //! the context of the error. Can be built from native error codes

        //#ifdef OSMACRO
        int native_;
        //#endif
        
    };

    /**
     * @brief The policy to use when shutting down (parts of) a socket
     * 
     * @see toNativeShutdownPolicy
     * @see SocketBase::shutdown
     */
    enum class ShutdownPolicy{
        SHUT_RECEPTIONS, //! Shuts down receptions. This means that the socket that you call this policy on won't be able to receive further information from it's peer
        SHUT_TRANSMISSIONS, //! Shuts down transmissions. This means that the socket that you call this policy on won't be able to send further information from it's peer
        SHUT_TRANSMISSIONS_AND_RECEPTIONS //! Does the same as the other two
    };

    /**
     * @brief Converts ninttp policy to native policy
     * 
     * @param what the policy to convert from
     * @return constexpr int representing the native code, or -1 if there isn't a match
     */
    constexpr int toNativeShutdownPolicy(ShutdownPolicy what){
        switch(what){
            case ShutdownPolicy::SHUT_RECEPTIONS:
                return SHUT_RD;
            case ShutdownPolicy::SHUT_TRANSMISSIONS:
                return SHUT_WR;
            case ShutdownPolicy::SHUT_TRANSMISSIONS_AND_RECEPTIONS:
                return SHUT_RDWR;
            default:
                return -1;
        }
    }


    /**
     * @brief Known policy-level errors used when validating library enums and conversions
     */
    enum class PolicyErrCode{
        UNKNOWN, //! unknown policy error
        UNRECOGNIZED_DOMAIN, //! the provided domain is not recognized by ninttp
        UNRECOGNIZED_SERVICE, //! the provided service is not recognized by ninttp
        UNRECOGNIZED_PROTOCOL, //! the provided protocol is not recognized by ninttp
        UNRECOGNIZED_SHUTDOWN_POLICY //! the provided shutdown policy is not recognized by ninttp
    };

    /**
     * @brief Represents a policy conversion/validation error
     */
    struct PolicyError{
        /**
         * @brief Builds a PolicyError from a policy error code and the location of the caller
         * @param policyError the policy error code
         * @param location Source location of the caller
         */
        PolicyError(PolicyErrCode policyError, std::source_location location = std::source_location::current()) noexcept 
            : location_(location), policyError_(policyError) {};

        /**
         * @brief Returns a comprehensive error message combining location and policy code description
         * @return The message composed from the caller function and the policy error description
         */
        constexpr std::string msg() const noexcept{
            return location_.function_name() + std::string(": ") + strFromPolicyError_(policyError_);
        }

        std::source_location location_; //! the source location of the caller
        PolicyErrCode policyError_; //! the policy-level error code

        private:

            /**
             * @brief Converts a policy error code into its human-readable description
             * @param pErr the policy error code
             * @return constexpr std::string with the textual description of @p pErr
             */
            constexpr static std::string strFromPolicyError_(PolicyErrCode pErr) noexcept{
                switch(pErr){
                    case PolicyErrCode::UNRECOGNIZED_DOMAIN:
                        return "unrecognized domain";
                    case PolicyErrCode::UNRECOGNIZED_SERVICE:
                        return "unrecognized service";
                    case PolicyErrCode::UNRECOGNIZED_PROTOCOL:
                        return "unrecognized protocol";
                    case PolicyErrCode::UNRECOGNIZED_SHUTDOWN_POLICY:
                        return "unrecognized socket shutdown policy";
                    case PolicyErrCode::UNKNOWN:
                    default:
                        return "unknown policy error";
                }
            }
    };
} // namespace ninttp
