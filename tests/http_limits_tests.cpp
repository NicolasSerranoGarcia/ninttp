#include <ninttp/http/http_limits.hpp>
#include <ninttp/http/internal/http_error_factory.hpp>
#include <ninttp/http/internal/http_request_parser.hpp>
#include <ninttp/http/internal/http_response_parser.hpp>

#include "test_check.hpp"
static_assert(ninttp::limits::MaxMethodLength == 17);
static_assert(ninttp::limits::MaxBodyLength == 8);
static_assert(ninttp::limits::MaxHeaderCount == 2);

#ifdef MaxMethodLength
    #error "HTTP limit override macros must not leak past http_limits.hpp"
#endif

int main(){
    {
        ninttp::internal::httpRequestParser<ninttp::http_1_1> parser;
        const auto result = parser.append(
            "GET / HTTP/1.1\r\n"
            "Host: example.test\r\n"
            "Accept: */*\r\n"
            "User-Agent: ninttp-test\r\n"
            "\r\n");

        NINTTP_CHECK(!result.has_value());
        NINTTP_CHECK(result.error().type ==
            ninttp::internal::httpParseErrorType::HeaderFieldsTooLarge);
        NINTTP_CHECK(ninttp::internal::httpErrorFactory<ninttp::http_1_1>
            ::statusCodeFromParseError(result.error().type) == 431);
    }

    {
        ninttp::internal::httpRequestParser<ninttp::http_1_1> parser;
        const auto result = parser.append(
            "POST / HTTP/1.1\r\n"
            "Host: example.test\r\n"
            "Content-Length: 9\r\n"
            "\r\n"
            "123456789");

        NINTTP_CHECK(!result.has_value());
        NINTTP_CHECK(result.error().type == ninttp::internal::httpParseErrorType::BodyTooLarge);
        NINTTP_CHECK(ninttp::internal::httpErrorFactory<ninttp::http_1_1>
            ::statusCodeFromParseError(result.error().type) == 413);
    }

    {
        ninttp::internal::httpResponseParser<ninttp::http_1_1> parser{"GET"};
        const auto result = parser.append(
            "HTTP/1.1 200 OK\r\n"
            "Content-Length: 9\r\n"
            "\r\n"
            "123456789");

        NINTTP_CHECK(!result.has_value());
        NINTTP_CHECK(result.error().type == ninttp::internal::httpParseErrorType::InvalidLength);
    }

    {
        ninttp::internal::httpRequestParser<ninttp::http_1_1> parser;
        const auto result = parser.append(
            "POST / HTTP/1.1\r\n"
            "Host: example.test\r\n"
            "Transfer-Encoding: chunked\r\n"
            "\r\n"
            "9\r\n123456789\r\n0\r\n\r\n");

        NINTTP_CHECK(!result.has_value());
        NINTTP_CHECK(result.error().type == ninttp::internal::httpParseErrorType::BodyTooLarge);
    }

    {
        ninttp::internal::httpRequestParser<ninttp::http_1_1> parser;
        const auto result = parser.append(
            "POST / HTTP/1.1\r\n"
            "Host: example.test\r\n"
            "Content-Length: nope\r\n"
            "\r\n");

        NINTTP_CHECK(!result.has_value());
        NINTTP_CHECK(result.error().type == ninttp::internal::httpParseErrorType::UnrecognizedToken);
        NINTTP_CHECK(ninttp::internal::httpErrorFactory<ninttp::http_1_1>
            ::statusCodeFromParseError(result.error().type) == 400);
    }
}
