#include <optional>
#include <string>

#include "socket/internal/select_backend.hpp"
#include "socket/socket_error.hpp"

#include "http/internal/http_parse_error.hpp"

namespace ninttp{
    
    enum class NinErrorType{
        Socket, 
        Parse
    };
    
    //swift style enum, where a .type has an associacted value, represented with an optional present only for that enum case
    struct NinError{
        NinErrorType type;

        std::optional<std::string> parseText;
        std::optional<internal::SelectedBackend::ErrorT> nativeError;

        //preallocated message comming from the underlying error. Must be preallocated by the caller so that allocation errors are handled before construction.
        std::string what;

        static NinError fromSocketError(const SocketError& err){
            return NinError{ .type = NinErrorType::Socket, .nativeError = err.errorCode(), .what = err.what()};
        }

        static NinError fromhttpParseError(const internal::httpParseError& err){
            return NinError{ .type = NinErrorType::Parse, .parseText = err.parseContextText, .what = err.what};
        }
    };
} //namespace ninttp