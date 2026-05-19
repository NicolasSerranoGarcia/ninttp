#pragma once

#include <string>
#include <optional>
#include <vector>
#include <cstddef>

namespace ninttp::internal{
    enum class httpMethod{
        GET,
        PUT,
        DELETE,
        PATCH,
        HEAD,
        INVALID
    };

    constexpr const httpMethod all_verbs[] = {httpMethod::GET, httpMethod::PUT, httpMethod::DELETE, httpMethod::PATCH, httpMethod::HEAD};
    constexpr const char* httpVerbStr[] = {"GET", "PUT", "DELETE", "PATCH", "HEAD"};

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

    struct Request{
        std::vector<internal::Header> headers;
        internal::httpMethod op = internal::httpMethod::INVALID;
        std::string resource;
        //...
    };

    struct httpVersion{
        static constexpr std::optional<httpVersion> fromHeader(const std::string& s){
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