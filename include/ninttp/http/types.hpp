#pragma once

#include <string>
#include <optional>
#include <vector>
#include <cstddef>
#include <ostream>

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

    constexpr const httpMethod all_verbs[] = {httpMethod::GET, httpMethod::PUT, httpMethod::DEL, httpMethod::PATCH, httpMethod::HEAD};
    constexpr const char* httpVerbStr[] = {"GET", "PUT", "DEL", "PATCH", "HEAD"};

    struct Header{
        std::string key;
        std::string value;
    };
} // namespace ninttp::internal

namespace ninttp
{
    struct Response{
        bool ok;
        std::optional<int> errCode;
        std::vector<internal::Header> headers;
        std::string contents;
        //...
    };

    //maybe wire the interfaces to only use this. For example, addHeader and so, then request builder would not be needed semantically
    struct Request{
        std::vector<internal::Header> headers;
        internal::httpMethod op = internal::httpMethod::INVALID;
        std::string resource;
        std::string body;
        //...
    };

    inline std::ostream& operator<<(std::ostream& os, const Request& request){
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

        if(!request.body.empty())
            os << '\n' << request.body;

        return os;
    }

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

        httpVersion() = delete;
        uint8_t major;
        uint8_t minor;
    };

    constexpr const httpVersion http_1_0(1,0);
} // namespace ninttp
