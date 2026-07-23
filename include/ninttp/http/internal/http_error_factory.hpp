#pragma once

#include "../types.hpp"
#include "http_parse_error.hpp"

namespace ninttp::internal{
    template <httpVersion ver>
    class httpErrorFactory{
        public:
            static std::string fromStatusCode(StatusCode code){
                Response response{ver, code};
                response.clearContent();
                return response.toString();
            }

            static std::string fromParseErrorType(httpParseErrorType type){
                return fromStatusCode(statusCodeFromParseError(type));
            }

            static std::string fromParseError(httpParseError err){
                return fromParseErrorType(err.type);
            }

            [[nodiscard]] static constexpr StatusCode statusCodeFromParseError(httpParseErrorType type) noexcept{
                switch(type){
                    case httpParseErrorType::UnsupportedVersion:
                        return 505;

                    case httpParseErrorType::TargetTooLong:
                        return 414;

                    default:
                        return 400;
                }
            }
    };
} //namespace ninttp::internal
