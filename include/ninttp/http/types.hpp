#pragma once

#include <string>
#include <optional>
#include <vector>

namespace ninttp::internal{
    enum class httpVerb{
        GET,
        PUT,
        //DELETE,
        PATCH,
    };

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
        internal::httpVerb operation;
        std::string resource;
        //...
    };

    
    class httpVersion{
        public:
        
        static constinit const httpVersion _1_0;
    };
} // namespace ninttp