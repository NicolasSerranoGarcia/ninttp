#include <cassert>

#include <ninttp/http/http_limits.hpp>
#include <ninttp/http/internal/http_request_parser.hpp>

static_assert(ninttp::limits::MaxMethodLength == 17);
static_assert(ninttp::limits::MaxBodyLength == 8);
static_assert(ninttp::limits::MaxHeaderCount == 1);

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
            "\r\n");

        assert(!result.has_value());
        assert(result.error().type == ninttp::internal::httpParseErrorType::InvalidLength);
    }

    {
        ninttp::internal::httpRequestParser<ninttp::http_1_1> parser;
        const auto result = parser.append(
            "POST / HTTP/1.1\r\n"
            "Host: example.test\r\n"
            "Content-Length: 9\r\n"
            "\r\n"
            "123456789");

        assert(!result.has_value());
        assert(result.error().type == ninttp::internal::httpParseErrorType::InvalidLength);
    }
}
