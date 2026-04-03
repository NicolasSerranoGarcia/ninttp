#pragma once

#include <string>
#include <netinet/in.h>
#include <sys/un.h>

#include <source_location>

namespace ninttp
{
    
    //.msg() can be verbose,so most of the times, if you want to handle the error, you may want to consult the nativeError_ code
    //which on unix platforms comes from errno, and handle each individually
    struct socketError{
        socketError(const char* context, std::source_location location = std::source_location::current()) noexcept 
            : location_(location), context_(context){}

        std::string msg() const noexcept{
            return location_.function_name() + std::string(": ") + context_;
        }

        
        std::source_location location_;
        std::string context_;

        //#ifdef OSMACRO
        int native_;
        //#endif
        
    };

    enum class ShutdownPolicy{
        SHUT_RECEPTIONS,
        SHUT_TRANSMISSIONS,
        SHUT_TRANSMISSIONS_AND_RECEPTIONS
    };

    int toNativeShutdownPolicy(ShutdownPolicy what){
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


    enum class PolicyErrCode{
        UNKNOWN,
        UNRECOGNIZED_DOMAIN,
        UNRECOGNIZED_SERVICE,
        UNRECOGNIZED_PROTOCOL,
        UNRECOGNIZED_SHUTDOWN_POLICY
    };

    struct PolicyError{
        PolicyError(PolicyErrCode policyError, std::source_location location = std::source_location::current()) noexcept 
            : location_(location), policyError_(policyError) {};

        std::string msg() const noexcept{
            return location_.function_name() + std::string(": ") + strFromPolicyError_(policyError_);
        }

        std::source_location location_;
        PolicyErrCode policyError_;

        static std::string strFromPolicyError_(PolicyErrCode pErr) noexcept{
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
