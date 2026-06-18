#pragma once

#include <string>
#include <optional>
#include <vector>
#include <cstddef>
#include <ostream>
#include <string_view>

namespace ninttp::internal{
    enum class httpMethod{
        GET,
        PUT,
        DEL, // windows defines a macro with the name DELETE. If we name this DELETE the compiler expands the macro before processing
        //the enum which breaks the code. This is the only reliable way I found without braking compatibility.
        PATCH,
        HEAD,
        INVALID
    };

    //only for iterating, this is why INVALID is not included
    constexpr const httpMethod all_verbs[] = {httpMethod::GET, httpMethod::PUT, httpMethod::DEL, httpMethod::PATCH, httpMethod::HEAD};
    constexpr const char* httpVerbStr[] = {"GET", "PUT", "DELETE", "PATCH", "HEAD"};

    struct Header{
        std::string key;
        std::string value;
    };
} // namespace ninttp::internal

namespace ninttp
{

    struct httpVersion{
        static constexpr std::optional<httpVersion> fromRequestLine(const std::string& s){
            if (s == "HTTP/0.9") return httpVersion{1,0};
            if (s == "HTTP/1.0") return httpVersion{1,0};
            if (s == "HTTP/1.1") return httpVersion{1,1};
            if (s == "HTTP/2.0") return httpVersion{1,2};
            if (s == "HTTP/3.0") return httpVersion{1,2};
            return std::nullopt;
        }

        constexpr httpVersion(uint8_t major, uint8_t minor) noexcept
            : major(major), minor(minor){}

        std::string toString() const noexcept{
            return std::to_string(major) + std::string(".") + std::to_string(minor);
        }

        std::string toHeaderString() const{
            return std::string("HTTP/") + toString();
        }

        httpVersion() noexcept = default;
        uint8_t major;
        uint8_t minor;
    };

    constexpr const httpVersion http_1_0(1,0);

    //maybe change name or put it as a method inside a statusCode struct with implicit conversions to int
    using StatusCode = int;
    [[nodiscard]] constexpr std::string_view getReadableStatus(StatusCode code) noexcept{
        switch(code){
            case 100: return "Continue";
            case 101: return "Switching Protocols";
            case 102: return "Processing";
            case 103: return "Early Hints";
            case 200: return "OK";
            case 201: return "Created";
            case 202: return "Accepted";
            case 203: return "Non-Authoritative Information";
            case 204: return "No Content";
            case 205: return "Reset Content";
            case 206: return "Partial Content";
            case 207: return "Multi-Status";
            case 208: return "Already Reported";
            case 226: return "IM Used";
            case 300: return "Multiple Choices";
            case 301: return "Moved Permanently";
            case 302: return "Found";
            case 303: return "See Other";
            case 304: return "Not Modified";
            case 305: return "Use Proxy";
            case 307: return "Temporary Redirect";
            case 308: return "Permanent Redirect";
            case 400: return "Bad Request";
            case 401: return "Unauthorized";
            case 402: return "Payment Required";
            case 403: return "Forbidden";
            case 404: return "Not Found";
            case 405: return "Method Not Allowed";
            case 406: return "Not Acceptable";
            case 407: return "Proxy Authentication Required";
            case 408: return "Request Timeout";
            case 409: return "Conflict";
            case 410: return "Gone";
            case 411: return "Length Required";
            case 412: return "Precondition Failed";
            case 413: return "Content Too Large";
            case 414: return "URI Too Long";
            case 415: return "Unsupported Media Type";
            case 416: return "Range Not Satisfiable";
            case 417: return "Expectation Failed";
            case 418: return "I'm a teapot";
            case 421: return "Misdirected Request";
            case 422: return "Unprocessable Content";
            case 423: return "Locked";
            case 424: return "Failed Dependency";
            case 425: return "Too Early";
            case 426: return "Upgrade Required";
            case 428: return "Precondition Required";
            case 429: return "Too Many Requests";
            case 431: return "Request Header Fields Too Large";
            case 451: return "Unavailable For Legal Reasons";
            case 500: return "Internal Server Error";
            case 501: return "Not Implemented";
            case 502: return "Bad Gateway";
            case 503: return "Service Unavailable";
            case 504: return "Gateway Timeout";
            case 505: return "HTTP Version Not Supported";
            case 506: return "Variant Also Negotiates";
            case 507: return "Insufficient Storage";
            case 508: return "Loop Detected";
            case 510: return "Not Extended";
            case 511: return "Network Authentication Required";
            default: return "Unknown Status";
        }
    }

    struct Response{
        httpVersion version;
        //them classic 200, 300... codes
        StatusCode statusCode;

        std::vector<internal::Header> headers;
        std::optional<std::string> body;
        //...
    };

    inline std::ostream& operator<<(std::ostream& os, const Response& response){
        os << response.version.toHeaderString() << ' '
           << response.statusCode << ' '
           << getReadableStatus(response.statusCode) << '\n';

        for(const auto& header : response.headers)
            os << header.key << ": " << header.value << '\n';

        if(response.body.has_value())
            os << '\n' << response.body.value();

        return os;
    }

    //maybe wire the interfaces to only use this. For example, addHeader and so, then request builder would not be needed semantically
    struct Request{
        internal::httpMethod op = internal::httpMethod::INVALID;
        std::string resource;
        httpVersion version; //maybe for semantic completeness?
        std::vector<internal::Header> headers;
        std::optional<std::string> body;

        void clear() noexcept{
            op = internal::httpMethod::INVALID;
            resource.clear();
        }
    };

    inline std::ostream& operator<<(std::ostream& os, const Request& request){
        //not use the str array bc this should account for invalid too, which the array does not have
        switch(request.op){
            case internal::httpMethod::GET:
                os << "GET";
                break;
            case internal::httpMethod::PUT:
                os << "PUT";
                break;
            case internal::httpMethod::DEL:
                os << "DELETE";
                break;
            case internal::httpMethod::PATCH:
                os << "PATCH";
                break;
            case internal::httpMethod::HEAD:
                os << "HEAD";
                break;
            case internal::httpMethod::INVALID:
                os << "INVALID";
                break;
        }

        os << ' ' << request.resource << '\n';

        for(const auto& header : request.headers)
            os << header.key << ": " << header.value << '\n';

        if(request.body.has_value())
            os << '\n' << request.body.value();

        return os;
    }
} // namespace ninttp
