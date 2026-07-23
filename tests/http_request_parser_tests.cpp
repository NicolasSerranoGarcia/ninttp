#include <cassert>
#include <string>

#include <ninttp/http/internal/http_request_parser.hpp>

using ninttp::http_1_1;
using ninttp::http_1_0;
using ninttp::internal::httpParseErrorType;
using ninttp::internal::httpParseStatus;
using ninttp::internal::httpRequestParser;

int main(){
    {
        httpRequestParser<http_1_1> parser;
        const auto result = parser.append(
            "POST /upload HTTP/1.1\r\n"
            "Host: example.test\r\n"
            "Transfer-Encoding: Chunked\r\n"
            "\r\n"
            "4\r\ntest\r\n"
            "0\r\n\r\n");

        assert(result == httpParseStatus::Done);
        assert(parser.getRequest().getContent() == "test");
    }

    {
        httpRequestParser<http_1_1> parser;
        const auto result = parser.append("GET / HTTP/1.0\r\n\r\n");
        assert(result == httpParseStatus::Done);
    }

    {
        httpRequestParser<http_1_1> parser;
        const auto result = parser.append("GET / HTTP/1.1\r\n\r\n");
        assert(!result.has_value());
        assert(result.error().type == httpParseErrorType::MissingHostHeader);
    }

    {
        httpRequestParser<http_1_1> parser;
        const auto result = parser.append("GET / HTTP/0.9\r\n\r\n");
        assert(!result.has_value());
        assert(result.error().type == httpParseErrorType::UnsupportedVersion);
    }

    {
        httpRequestParser<http_1_1> parser;
        const auto result = parser.append(
            "POST / HTTP/1.0\r\n"
            "Transfer-Encoding: chunked\r\n"
            "\r\n");
        assert(!result.has_value());
    }

    {
        httpRequestParser<http_1_1> parser;
        const auto result = parser.append("GET /\tbad HTTP/1.1\r\nHost: example.test\r\n\r\n");
        assert(!result.has_value());
        assert(result.error().type == httpParseErrorType::DisallowedTokenChar);
    }

    {
        httpRequestParser<http_1_1> parser;
        std::string request =
            "POST / HTTP/1.1\r\n"
            "Host: example.test\r\n"
            "Content-Length: 9000\r\n"
            "\r\n";
        request.append(9000, 'x');

        const auto result = parser.append(request);
        assert(result == httpParseStatus::Done);
        assert(parser.getRequest().getContent()->size() == 9000);
    }

    {
        ninttp::Request request;
        assert(!request.setContent("test", ninttp::RequestBodyFraming::Chunked));
        assert(request.setVersion(http_1_1));
        assert(!request.hasConsistentBodyFraming());
        assert(request.setHeader("host", "example.test"));
        assert(request.setContent("test", ninttp::RequestBodyFraming::Chunked));
        assert(request.hasConsistentBodyFraming());
        assert(!request.setVersion(http_1_0));
        assert(request.getVersion().minor == 1);
    }

    {
        ninttp::Request request;
        assert(!request.setVersion(ninttp::httpVersion{2, 0}));

        ninttp::Response response;
        assert(!response.setVersion(ninttp::httpVersion{2, 0}));
    }
}
