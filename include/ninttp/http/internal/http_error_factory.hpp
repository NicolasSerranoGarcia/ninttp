#include <unordered_map>
#include <string_view>

#include "../types.hpp"
#include "http_parse_error.hpp"

namespace ninttp::internal{
    class httpErrorFactory{
        public:
            static std::string fromStatusCode(StatusCode code, httpVersion ver){
                Response response{ .version = ver, .statusCode = code };
                return response.toString();
            }

            static std::string fromParseErrorType(httpParseErrorType type, httpVersion ver){
                return fromStatusCode(statusCodeFromParseError(type), ver);
            }

            static std::string fromParseError(httpParseError err, httpVersion ver){
                return fromParseErrorType(err.type, ver);
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
