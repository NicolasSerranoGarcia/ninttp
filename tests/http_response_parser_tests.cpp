#include <cassert>
#include <string>
#include <string_view>

#include <ninttp/http/internal/http_response_parser.hpp>

using ninttp::ResponseBodyFraming;
using ninttp::http_1_1;
using ninttp::internal::httpParseErrorType;
using ninttp::internal::httpParseStatus;
using ninttp::internal::httpResponseParser;

template<typename Parser>
static auto appendOneByteAtATime(Parser& parser, std::string_view message){
    std::expected<httpParseStatus, ninttp::internal::httpParseError> result =
        httpParseStatus::NeedData;

    for(const char byte : message){
        result = parser.append(std::string(1, byte));
        if(!result.has_value() || *result == httpParseStatus::Done)
            break;
    }

    return result;
}

int main(){
    {
        httpResponseParser<http_1_1> parser{"GET"};
        const auto result = appendOneByteAtATime(
            parser,
            "HTTP/1.1 200 OK\r\n"
            "content-length: 5\r\n"
            "X-Test: value\r\n"
            "\r\n"
            "hello");

        assert(result == httpParseStatus::Done);
        auto response = parser.getResponse();
        assert(response.getStatusCode() == 200);
        assert(response.getBodyFraming() == ResponseBodyFraming::ContentLength);
        assert(response.getContent() == "hello");
        assert(response.getHeaders().size() == 2);
        assert(response.getHeaders()[1].name == "x-test");
        assert(response.getHeaders()[1].value == "value");
    }

    {
        httpResponseParser<http_1_1> parser{"GET"};
        const auto result = appendOneByteAtATime(
            parser,
            "HTTP/1.1 200 OK\r\n"
            "Transfer-Encoding: Chunked\r\n"
            "\r\n"
            "4;checksum=\"future\"\r\n"
            "test\r\n"
            "0\r\n"
            "Checksum: accepted\r\n"
            "\r\n");

        assert(result == httpParseStatus::Done);

        auto response = parser.getResponse();
        assert(response.getBodyFraming() == ResponseBodyFraming::Chunked);
        assert(response.getContent() == "test");
        assert(response.getTrailingHeaders().has_value());
        assert(response.getTrailingHeaders()->size() == 1);
        assert(response.getTrailingHeaders()->front().name == "checksum");
        assert(response.getTrailingHeaders()->front().value == "accepted");
    }

    {
        httpResponseParser<http_1_1> parser{"GET"};
        auto result = parser.append(
            "HTTP/1.0 200 OK\r\n"
            "Content-Type: text/plain\r\n"
            "\r\n"
            "close-delimited");

        assert(result == httpParseStatus::NeedData);
        result = parser.finish();
        assert(result == httpParseStatus::Done);

        auto response = parser.getResponse();
        assert(response.getBodyFraming() == ResponseBodyFraming::ConnectionClose);
        assert(response.getContent() == "close-delimited");
    }

    {
        httpResponseParser<http_1_1> parser{"HEAD"};
        const auto result = parser.append(
            "HTTP/1.1 200 OK\r\n"
            "Content-Length: 100\r\n"
            "\r\n"
            "NEXT");

        assert(result == httpParseStatus::Done);
        assert(parser.getLeftoverBytes() == "NEXT");
        const auto response = parser.getResponse();
        assert(response.getBodyFraming() == ResponseBodyFraming::None);
        assert(!response.getContent().has_value());
    }

    {
        httpResponseParser<http_1_1> parser{"CONNECT"};
        const auto result = parser.append(
            "HTTP/1.1 200 Connection Established\r\n"
            "\r\n"
            "tunnel bytes");

        assert(result == httpParseStatus::Done);
        assert(parser.getLeftoverBytes() == "tunnel bytes");
        assert(parser.getResponse().getBodyFraming() == ResponseBodyFraming::Tunnel);
    }

    {
        httpResponseParser<http_1_1> parser{"GET"};
        const auto result = parser.append(
            "HTTP/1.1 101 Switching Protocols\r\n"
            "Connection: Upgrade\r\n"
            "Upgrade: websocket\r\n"
            "\r\n"
            "upgraded bytes");

        assert(result == httpParseStatus::Done);
        assert(parser.getLeftoverBytes() == "upgraded bytes");
        assert(parser.getResponse().getBodyFraming() == ResponseBodyFraming::Tunnel);
    }

    {
        httpResponseParser<http_1_1> parser{"GET"};
        const auto result = parser.append(
            "HTTP/1.1 204 No Content\r\n"
            "Content-Length: 10\r\n"
            "\r\n"
            "NEXT");

        assert(result == httpParseStatus::Done);
        assert(parser.getLeftoverBytes() == "NEXT");
        assert(parser.getResponse().getBodyFraming() == ResponseBodyFraming::None);
    }

    {
        httpResponseParser<http_1_1> parser{"GET"};
        const auto result = parser.append(
            "HTTP/1.0 200 OK\r\n"
            "Transfer-Encoding: chunked\r\n"
            "\r\n");

        assert(!result.has_value());
        assert(result.error().type == httpParseErrorType::UnexpectedToken);
    }

    {
        httpResponseParser<http_1_1> parser{"GET"};
        const auto result = parser.append(
            "HTTP/1.1 200 OK\r\n"
            "Content-Length: 4\r\n"
            "Transfer-Encoding: chunked\r\n"
            "\r\n");

        assert(!result.has_value());
        assert(result.error().type == httpParseErrorType::IncompatibleHeaders);
    }

    {
        httpResponseParser<http_1_1> parser{"GET"};
        auto result = parser.append(
            "HTTP/1.1 200 OK\r\n"
            "Content-Length: 5\r\n"
            "\r\n"
            "abc");

        assert(result == httpParseStatus::NeedData);
        result = parser.finish();
        assert(!result.has_value());
        assert(result.error().type == httpParseErrorType::ExpectedMissingToken);
    }

    {
        httpResponseParser<http_1_1> parser{"GET"};
        const auto result = parser.append(
            "HTTP/1.1 200 OK\r\n"
            "Content-Length: 4\r\n"
            "Content-Length: 5\r\n"
            "\r\n");

        assert(!result.has_value());
        assert(result.error().type == httpParseErrorType::DuplicatedHeader);
    }

    {
        ninttp::Response response;
        assert(response.setVersion(http_1_1));
        response.setStatusCode(200);
        assert(!response.setContent("not an HTTP body", ResponseBodyFraming::Tunnel));
        assert(response.setContent("hello", ResponseBodyFraming::Chunked));
        assert(response.addTrailingHeader("Checksum", "accepted"));

        httpResponseParser<http_1_1> parser{"GET"};
        const auto result = parser.append(response.toString());
        assert(result == httpParseStatus::Done);

        auto parsed = parser.getResponse();
        assert(parsed.getContent() == "hello");
        assert(parsed.getTrailingHeaders().has_value());
        assert(parsed.getTrailingHeaders()->front().value == "accepted");
    }
}
