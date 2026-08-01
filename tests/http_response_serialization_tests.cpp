#include <string>

#include <ninttp/http/types.hpp>

#include "test_check.hpp"

namespace
{
    bool contains(const std::string& value, const std::string& expected){
        return value.find(expected) != std::string::npos;
    }
}

int main(){
    {
        ninttp::Response response{ninttp::http_1_1, 200};
        NINTTP_CHECK(response.setContent("hello"));

        const auto serialized = response.toString("HEAD");
        NINTTP_CHECK(contains(serialized, "Content-Length: 5\r\n"));
        NINTTP_CHECK(!contains(serialized, "\r\n\r\nhello"));
    }

    for(const ninttp::StatusCode status : {100, 204, 205}){
        ninttp::Response response{ninttp::http_1_1, status};
        NINTTP_CHECK(response.setContent("forbidden"));

        const auto serialized = response.toString("GET");
        NINTTP_CHECK(!contains(serialized, "Content-Length:"));
        NINTTP_CHECK(!contains(serialized, "Transfer-Encoding:"));
        NINTTP_CHECK(!contains(serialized, "forbidden"));
    }

    {
        ninttp::Response response{ninttp::http_1_1, 304};
        NINTTP_CHECK(response.setContent("cached"));

        const auto serialized = response.toString("GET");
        NINTTP_CHECK(contains(serialized, "Content-Length: 6\r\n"));
        NINTTP_CHECK(!contains(serialized, "\r\n\r\ncached"));
    }

    {
        ninttp::Response response{ninttp::http_1_1, 200};
        NINTTP_CHECK(response.setContent("tunnel data"));

        const auto serialized = response.toString("CONNECT");
        NINTTP_CHECK(!contains(serialized, "Content-Length:"));
        NINTTP_CHECK(!contains(serialized, "tunnel data"));
    }

    {
        ninttp::Response response{ninttp::http_1_1, 411};
        response.clearContent();
        const auto serialized = response.toString("POST");
        NINTTP_CHECK(contains(serialized, "HTTP/1.1 411 Length Required\r\n"));
    }
}
