#include <string>
#include <string_view>

#include <ninttp/http/internal/http_response_parser.hpp>

#include "test_check.hpp"
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

        NINTTP_CHECK(result == httpParseStatus::Done);
        auto response = parser.getResponse();
        NINTTP_CHECK(response.getStatusCode() == 200);
        NINTTP_CHECK(response.getBodyFraming() == ResponseBodyFraming::ContentLength);
        NINTTP_CHECK(response.getContent() == "hello");
        NINTTP_CHECK(response.getHeaders().size() == 2);
        NINTTP_CHECK(response.getHeaders()[1].name == "x-test");
        NINTTP_CHECK(response.getHeaders()[1].value == "value");
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

        NINTTP_CHECK(result == httpParseStatus::Done);

        auto response = parser.getResponse();
        NINTTP_CHECK(response.getBodyFraming() == ResponseBodyFraming::Chunked);
        NINTTP_CHECK(response.getContent() == "test");
        NINTTP_CHECK(response.getTrailingHeaders().has_value());
        NINTTP_CHECK(response.getTrailingHeaders()->size() == 1);
        NINTTP_CHECK(response.getTrailingHeaders()->front().name == "checksum");
        NINTTP_CHECK(response.getTrailingHeaders()->front().value == "accepted");
    }

    {
        httpResponseParser<http_1_1> parser{"GET"};
        auto result = parser.append(
            "HTTP/1.0 200 OK\r\n"
            "Content-Type: text/plain\r\n"
            "\r\n"
            "close-delimited");

        NINTTP_CHECK(result == httpParseStatus::NeedData);
        result = parser.finish();
        NINTTP_CHECK(result == httpParseStatus::Done);

        auto response = parser.getResponse();
        NINTTP_CHECK(response.getBodyFraming() == ResponseBodyFraming::ConnectionClose);
        NINTTP_CHECK(response.getContent() == "close-delimited");
    }

    {
        httpResponseParser<http_1_1> parser{"HEAD"};
        const auto result = parser.append(
            "HTTP/1.1 200 OK\r\n"
            "Content-Length: 100\r\n"
            "\r\n"
            "NEXT");

        NINTTP_CHECK(result == httpParseStatus::Done);
        NINTTP_CHECK(parser.getLeftoverBytes() == "NEXT");
        const auto response = parser.getResponse();
        NINTTP_CHECK(response.getBodyFraming() == ResponseBodyFraming::None);
        NINTTP_CHECK(!response.getContent().has_value());
    }

    {
        httpResponseParser<http_1_1> parser{"CONNECT"};
        const auto result = parser.append(
            "HTTP/1.1 200 Connection Established\r\n"
            "\r\n"
            "tunnel bytes");

        NINTTP_CHECK(result == httpParseStatus::Done);
        NINTTP_CHECK(parser.getLeftoverBytes() == "tunnel bytes");
        NINTTP_CHECK(parser.getResponse().getBodyFraming() == ResponseBodyFraming::Tunnel);
    }

    {
        httpResponseParser<http_1_1> parser{"GET"};
        const auto result = parser.append(
            "HTTP/1.1 101 Switching Protocols\r\n"
            "Connection: Upgrade\r\n"
            "Upgrade: websocket\r\n"
            "\r\n"
            "upgraded bytes");

        NINTTP_CHECK(result == httpParseStatus::Done);
        NINTTP_CHECK(parser.getLeftoverBytes() == "upgraded bytes");
        NINTTP_CHECK(parser.getResponse().getBodyFraming() == ResponseBodyFraming::Tunnel);
    }

    {
        httpResponseParser<http_1_1> parser{"GET"};
        const auto result = parser.append(
            "HTTP/1.1 204 No Content\r\n"
            "Content-Length: 10\r\n"
            "\r\n"
            "NEXT");

        NINTTP_CHECK(result == httpParseStatus::Done);
        NINTTP_CHECK(parser.getLeftoverBytes() == "NEXT");
        NINTTP_CHECK(parser.getResponse().getBodyFraming() == ResponseBodyFraming::None);
    }

    {
        httpResponseParser<http_1_1> parser{"GET"};
        const auto result = parser.append(
            "HTTP/1.0 200 OK\r\n"
            "Transfer-Encoding: chunked\r\n"
            "\r\n");

        NINTTP_CHECK(!result.has_value());
        NINTTP_CHECK(result.error().type == httpParseErrorType::UnexpectedToken);
    }

    {
        httpResponseParser<http_1_1> parser{"GET"};
        const auto result = parser.append(
            "HTTP/1.1 200 OK\r\n"
            "Content-Length: 4\r\n"
            "Transfer-Encoding: chunked\r\n"
            "\r\n");

        NINTTP_CHECK(!result.has_value());
        NINTTP_CHECK(result.error().type == httpParseErrorType::IncompatibleHeaders);
    }

    {
        httpResponseParser<http_1_1> parser{"GET"};
        auto result = parser.append(
            "HTTP/1.1 200 OK\r\n"
            "Content-Length: 5\r\n"
            "\r\n"
            "abc");

        NINTTP_CHECK(result == httpParseStatus::NeedData);
        result = parser.finish();
        NINTTP_CHECK(!result.has_value());
        NINTTP_CHECK(result.error().type == httpParseErrorType::ExpectedMissingToken);
    }

    {
        httpResponseParser<http_1_1> parser{"GET"};
        const auto result = parser.append(
            "HTTP/1.1 200 OK\r\n"
            "Content-Length: 4\r\n"
            "Content-Length: 5\r\n"
            "\r\n");

        NINTTP_CHECK(!result.has_value());
        NINTTP_CHECK(result.error().type == httpParseErrorType::DuplicatedHeader);
    }

    {
        ninttp::Response response;
        NINTTP_CHECK(response.setVersion(http_1_1));
        response.setStatusCode(200);
        NINTTP_CHECK(!response.setContent("not an HTTP body", ResponseBodyFraming::Tunnel));
        NINTTP_CHECK(response.setContent("hello", ResponseBodyFraming::Chunked));
        NINTTP_CHECK(response.addTrailingHeader("Checksum", "accepted"));

        httpResponseParser<http_1_1> parser{"GET"};
        const auto result = parser.append(response.toString());
        NINTTP_CHECK(result == httpParseStatus::Done);

        auto parsed = parser.getResponse();
        NINTTP_CHECK(parsed.getContent() == "hello");
        NINTTP_CHECK(parsed.getTrailingHeaders().has_value());
        NINTTP_CHECK(parsed.getTrailingHeaders()->front().value == "accepted");
    }
}
