#pragma once

#include <cassert>
#include <charconv>
#include <cstddef>
#include <expected>
#include <iostream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#include "../http_limits.hpp"
#include "../types.hpp"
#include "http_parse_error.hpp"
#include "parse_utils.hpp"

namespace ninttp::internal
{
    template<httpVersion ver = http_1_0>
    class httpResponseParser{
        static_assert(isSupportedHTTP1Version(ver),
            "HTTP response parser only supports HTTP/1.0 and HTTP/1.1");

        enum class Processing{
            StatusLine,
            Headers,
            Body,
            TrailingHeaders,
            Finished
        };

        enum class BodyFraming{
            None,
            ContentLength,
            Chunked,
            ConnectionClose,
            Tunnel
        };

        public:
            explicit constexpr httpResponseParser(std::string_view requestMethod = {}) noexcept
                : responseToHead(requestMethod == "HEAD"),
                  responseToConnect(requestMethod == "CONNECT"){}

            std::expected<httpParseStatus, httpParseError> append(const std::string& received){
                assert(state != Processing::Finished);
                constructed.append(received);

                while(true){
                    switch(state){
                        case Processing::StatusLine:{
                            const auto lineEnd = constructed.find("\r\n");
                            if(lineEnd == std::string::npos){
                                if(constructed.size() > limits::MaxStatusLineLength)
                                    return lengthLimitError("Status line exceeds configured maximum length");

                                return httpParseStatus::NeedData;
                            }

                            if(lineEnd + 2 > limits::MaxStatusLineLength)
                                return lengthLimitError("Status line exceeds configured maximum length");

                            const auto line = std::string_view{constructed}.substr(0, lineEnd);
                            const auto versionEnd = line.find(' ');
                            if(versionEnd == std::string_view::npos)
                                return malformedStatusLine(line);

                            const auto statusStart = versionEnd + 1;
                            const auto statusEnd = line.find(' ', statusStart);
                            if(statusEnd == std::string_view::npos || statusEnd - statusStart != 3)
                                return malformedStatusLine(line);

                            const auto versionText = line.substr(0, versionEnd);
                            if(versionText.size() > limits::HTTPVersionLength)
                                return std::unexpected{httpParseError{
                                    .type = httpParseErrorType::VersionTooLong,
                                    .parseContextText = std::string(versionText),
                                    .what = "Response version token exceeds configured maximum length"
                                }};

                            const auto parsedVersion = httpVersion::fromRequestLineVersion(versionText);
                            if(!parsedVersion.has_value() || parsedVersion->major != 1)
                                return std::unexpected{httpParseError{
                                    .type = httpParseErrorType::UnsupportedVersion,
                                    .parseContextText = std::string(versionText),
                                    .what = "Response does not use a supported HTTP/1.x version"
                                }};

                            StatusCode status = 0;
                            const auto statusText = line.substr(statusStart, 3);
                            const char* first = statusText.data();
                            const char* last = first + statusText.size();
                            const auto [ptr, ec] = std::from_chars(first, last, status);
                            if(ec != std::errc{} || ptr != last || status < 100)
                                return malformedStatusLine(line);

                            const auto reason = line.substr(statusEnd + 1);
                            for(const char c : reason){
                                const auto byte = static_cast<unsigned char>(c);
                                if(c != '\t' && c != ' ' &&
                                    !(byte >= 0x21 && byte <= 0x7e) &&
                                    byte < 0x80)
                                    return malformedStatusLine(line);
                            }

                            response.version = *parsedVersion;
                            response.statusCode = status;
                            lastProcessedIdx = lineEnd + 2;
                            state = Processing::Headers;
                            break;
                        }

                        case Processing::Headers:{
                            std::size_t currentHeaderEnd;

                            while(true){
                                if(currentHeaderEnd = constructed.find("\r\n", lastProcessedIdx);
                                    currentHeaderEnd == std::string::npos)
                                {
                                    const auto pendingLength = constructed.size() - lastProcessedIdx;
                                    if(pendingLength > limits::MaxHeaderLineLength ||
                                        headerSectionLength + pendingLength > limits::MaxHeaderSectionLength)
                                        return lengthLimitError("Header section exceeds configured limits");

                                    return httpParseStatus::NeedData;
                                }

                                const auto headerLineLength = currentHeaderEnd - lastProcessedIdx;
                                headerSectionLength += headerLineLength + 2;

                                if(headerLineLength > limits::MaxHeaderLineLength ||
                                    headerSectionLength > limits::MaxHeaderSectionLength)
                                    return lengthLimitError("Header section exceeds configured limits");

                                if(lastProcessedIdx == currentHeaderEnd){
                                    lastProcessedIdx += 2;
                                    break;
                                }

                                if(++headerCount > limits::MaxHeaderCount)
                                    return lengthLimitError("Header count exceeds configured maximum");

                                auto header = getHeader(currentHeaderEnd);
                                if(!header.has_value())
                                    return std::unexpected{std::move(header).error()};

                                auto parsedHeader = std::move(header).value();
                                parsedHeader.name = utils::toLower(std::move(parsedHeader.name));

                                if((parsedHeader.name == "content-length" && hasHeader("transfer-encoding")) ||
                                    (parsedHeader.name == "transfer-encoding" && hasHeader("content-length")))
                                {
                                    return std::unexpected{httpParseError{
                                        .type = httpParseErrorType::IncompatibleHeaders,
                                        .parseContextText = parsedHeader.name + ": " + parsedHeader.value,
                                        .what = "Response contains both Content-Length and Transfer-Encoding headers"
                                    }};
                                }

                                if(parsedHeader.name == "content-length"){
                                    const char* first = parsedHeader.value.data();
                                    const char* last = first + parsedHeader.value.size();
                                    const auto [ptr, ec] = std::from_chars(first, last, bodySize);

                                    if(ec == std::errc::invalid_argument || ptr != last)
                                        return std::unexpected{httpParseError{
                                            .type = httpParseErrorType::UnrecognizedToken,
                                            .parseContextText = parsedHeader.value,
                                            .what = "Expected a valid Content-Length value"
                                        }};

                                    if(ec == std::errc::result_out_of_range)
                                        return lengthLimitError("Content-Length exceeds the supported integer range");

                                    if(bodySize > limits::MaxBodyLength)
                                        return lengthLimitError("Content-Length exceeds configured maximum body length");

                                    if(const auto existing = findHeader("content-length");
                                        existing != response.headers.end())
                                    {
                                        if(existing->value != parsedHeader.value)
                                            return std::unexpected{httpParseError{
                                                .type = httpParseErrorType::DuplicatedHeader,
                                                .parseContextText = parsedHeader.value,
                                                .what = "Content-Length appears more than once with different values"
                                            }};

                                        continue;
                                    }

                                    bodyFramingType = BodyFraming::ContentLength;
                                    response.bodyFraming = ResponseBodyFraming::ContentLength;
                                } else if(parsedHeader.name == "transfer-encoding"){
                                    if(!usesHTTP11Rules(response.version))
                                        return std::unexpected{httpParseError{
                                            .type = httpParseErrorType::UnexpectedToken,
                                            .parseContextText = parsedHeader.value,
                                            .what = "Transfer-Encoding is not valid in an HTTP/1.0 response"
                                        }};

                                    if(hasHeader("transfer-encoding"))
                                        return std::unexpected{httpParseError{
                                            .type = httpParseErrorType::DuplicatedHeader,
                                            .parseContextText = parsedHeader.value,
                                            .what = "Transfer-Encoding header appears more than once"
                                        }};

                                    auto transferEncoding = parseTransferEncoding(parsedHeader.value);
                                    if(!transferEncoding.has_value())
                                        return std::unexpected{std::move(transferEncoding).error()};

                                    parsedHeader.value = "chunked";
                                    bodyFramingType = BodyFraming::Chunked;
                                    response.bodyFraming = ResponseBodyFraming::Chunked;
                                }

                                response.headers.push_back(std::move(parsedHeader));
                            }

                            const bool statusForbidsBody =
                                response.statusCode / 100 == 1 ||
                                response.statusCode == 204 ||
                                response.statusCode == 304;

                            if(response.statusCode == 101){
                                bodyFramingType = BodyFraming::Tunnel;
                                response.bodyFraming = ResponseBodyFraming::Tunnel;
                                return finishWithLeftovers();
                            }

                            if(responseToHead || statusForbidsBody){
                                bodyFramingType = BodyFraming::None;
                                response.bodyFraming = ResponseBodyFraming::None;
                                return finishWithLeftovers();
                            }

                            if(responseToConnect &&
                                response.statusCode >= 200 && response.statusCode < 300)
                            {
                                bodyFramingType = BodyFraming::Tunnel;
                                response.bodyFraming = ResponseBodyFraming::Tunnel;
                                return finishWithLeftovers();
                            }

                            if(bodyFramingType == BodyFraming::ContentLength && bodySize == 0)
                                return finishWithLeftovers();

                            if(bodyFramingType == BodyFraming::None){
                                bodyFramingType = BodyFraming::ConnectionClose;
                                response.bodyFraming = ResponseBodyFraming::ConnectionClose;
                            }

                            response.body.emplace();
                            remaining = bodySize;
                            state = Processing::Body;
                            break;
                        }

                        case Processing::Body:{
                            switch(bodyFramingType){
                                case BodyFraming::ContentLength:{
                                    const auto arrived = std::string_view{constructed}.substr(lastProcessedIdx);
                                    if(arrived.size() < remaining){
                                        response.body->append(arrived);
                                        remaining -= arrived.size();
                                        lastProcessedIdx += arrived.size();
                                        return httpParseStatus::NeedData;
                                    }

                                    response.body->append(arrived.substr(0, remaining));
                                    lastProcessedIdx += remaining;
                                    remaining = 0;
                                    return finishWithLeftovers();
                                }

                                case BodyFraming::Chunked:{
                                    const auto arrived = std::string_view{constructed}.substr(lastProcessedIdx);
                                    if(chunkedEncodingIsCurrentlyLength){
                                        const auto crlf = arrived.find("\r\n");
                                        if(crlf == std::string_view::npos){
                                            if(arrived.size() > limits::MaxChunkLineLength)
                                                return lengthLimitError("Chunk-size line exceeds configured maximum length");

                                            return httpParseStatus::NeedData;
                                        }

                                        if(crlf > limits::MaxChunkLineLength)
                                            return lengthLimitError("Chunk-size line exceeds configured maximum length");

                                        const auto chunkLine = arrived.substr(0, crlf);
                                        const char* first = chunkLine.data();
                                        const char* last = first + chunkLine.size();
                                        const auto [ptr, ec] =
                                            std::from_chars(first, last, chunkedEncodingLastLength, 16);

                                        if(ec == std::errc::invalid_argument)
                                            return std::unexpected{httpParseError{
                                                .type = httpParseErrorType::UnrecognizedToken,
                                                .parseContextText = std::string(chunkLine),
                                                .what = "Expected a valid hexadecimal chunk length"
                                            }};

                                        if(ec == std::errc::result_out_of_range)
                                            return lengthLimitError("Chunk length exceeds the supported integer range");

                                        const auto extensions =
                                            std::string_view{ptr, static_cast<std::size_t>(last - ptr)};
                                        auto parsedExtensions = parseChunkExtensions(extensions);
                                        if(!parsedExtensions.has_value())
                                            return std::unexpected{std::move(parsedExtensions).error()};

                                        if(chunkedEncodingLastLength > limits::MaxChunkLength)
                                            return lengthLimitError("Chunk length exceeds configured maximum");

                                        if(chunkedEncodingLastLength >
                                            limits::MaxBodyLength - response.body->size())
                                            return lengthLimitError("Decoded body exceeds configured maximum length");

                                        chunkedEncodingRemainingBytesForConsume =
                                            chunkedEncodingLastLength;
                                        lastProcessedIdx += crlf + 2;
                                        chunkedEncodingIsCurrentlyLength = false;

                                        if(chunkedEncodingLastLength == 0)
                                            state = Processing::TrailingHeaders;
                                    } else{
                                        if(chunkedEncodingRemainingBytesForConsume > arrived.size()){
                                            response.body->append(arrived);
                                            chunkedEncodingRemainingBytesForConsume -= arrived.size();
                                            lastProcessedIdx += arrived.size();
                                            return httpParseStatus::NeedData;
                                        }

                                        response.body->append(
                                            arrived.substr(0, chunkedEncodingRemainingBytesForConsume));
                                        lastProcessedIdx += chunkedEncodingRemainingBytesForConsume;
                                        const auto afterChunk = arrived.substr(
                                            chunkedEncodingRemainingBytesForConsume);
                                        chunkedEncodingRemainingBytesForConsume = 0;

                                        if(afterChunk.size() < 2)
                                            return httpParseStatus::NeedData;

                                        if(!afterChunk.starts_with("\r\n"))
                                            return std::unexpected{httpParseError{
                                                .type = httpParseErrorType::UnexpectedToken,
                                                .parseContextText = std::string(afterChunk),
                                                .what = "Expected CRLF after chunk data"
                                            }};

                                        lastProcessedIdx += 2;
                                        chunkedEncodingIsCurrentlyLength = true;
                                    }
                                    break;
                                }

                                case BodyFraming::ConnectionClose:{
                                    const auto arrived = std::string_view{constructed}.substr(lastProcessedIdx);
                                    if(arrived.size() > limits::MaxBodyLength - response.body->size())
                                        return lengthLimitError("Connection-close body exceeds configured maximum length");

                                    response.body->append(arrived);
                                    lastProcessedIdx += arrived.size();
                                    return httpParseStatus::NeedData;
                                }

                                case BodyFraming::None:
                                case BodyFraming::Tunnel:
                                    return finishWithLeftovers();
                            }
                            break;
                        }

                        case Processing::TrailingHeaders:{
                            std::size_t currentHeaderEnd;

                            while(true){
                                if(currentHeaderEnd = constructed.find("\r\n", lastProcessedIdx);
                                    currentHeaderEnd == std::string::npos)
                                {
                                    const auto pendingLength = constructed.size() - lastProcessedIdx;
                                    if(pendingLength > limits::MaxTrailerLineLength ||
                                        trailerSectionLength + pendingLength >
                                            limits::MaxTrailerSectionLength)
                                        return lengthLimitError("Trailer section exceeds configured limits");

                                    return httpParseStatus::NeedData;
                                }

                                const auto trailerLineLength = currentHeaderEnd - lastProcessedIdx;
                                trailerSectionLength += trailerLineLength + 2;

                                if(trailerLineLength > limits::MaxTrailerLineLength ||
                                    trailerSectionLength > limits::MaxTrailerSectionLength)
                                    return lengthLimitError("Trailer section exceeds configured limits");

                                if(lastProcessedIdx == currentHeaderEnd){
                                    lastProcessedIdx += 2;
                                    return finishWithLeftovers();
                                }

                                if(++trailerCount > limits::MaxTrailerCount)
                                    return lengthLimitError("Trailer field count exceeds configured maximum");

                                auto header = getHeader(currentHeaderEnd);
                                if(!header.has_value())
                                    return std::unexpected{std::move(header).error()};

                                auto parsedHeader = std::move(header).value();
                                parsedHeader.name = utils::toLower(std::move(parsedHeader.name));

                                if(!response.trailingHeaders.has_value())
                                    response.trailingHeaders.emplace();

                                response.trailingHeaders->push_back(std::move(parsedHeader));
                            }
                        }

                        case Processing::Finished:
                            return httpParseStatus::Done;
                    }
                }
            }

            std::expected<httpParseStatus, httpParseError> finish(){
                if(state == Processing::Finished)
                    return httpParseStatus::Done;

                if(state == Processing::Body &&
                    bodyFramingType == BodyFraming::ConnectionClose)
                {
                    state = Processing::Finished;
                    return httpParseStatus::Done;
                }

                return std::unexpected{httpParseError{
                    .type = httpParseErrorType::ExpectedMissingToken,
                    .parseContextText = contextFrom(lastProcessedIdx),
                    .what = "Connection closed before the response framing was complete"
                }};
            }

            [[nodiscard]] bool finished() const noexcept{
                return state == Processing::Finished;
            }

            [[nodiscard]] bool processingBody() const noexcept{
                return state == Processing::Body;
            }

            [[nodiscard]] bool processingHeaders() const noexcept{
                return state == Processing::Headers;
            }

            [[nodiscard]] bool processingStatusLine() const noexcept{
                return state == Processing::StatusLine;
            }

            Response getResponse() noexcept{
                assert(state == Processing::Finished);
                return std::move(response);
            }

            [[nodiscard]] bool hasLeftoverBytes() const noexcept{
                assert(state == Processing::Finished);
                return !leftoverBytes.empty();
            }

            std::string getLeftoverBytes() noexcept{
                assert(state == Processing::Finished);
                return std::move(leftoverBytes);
            }

            void reset() noexcept{
                constructed.clear();
                lastProcessedIdx = 0;
                bodySize = 0;
                remaining = 0;
                state = Processing::StatusLine;
                bodyFramingType = BodyFraming::None;
                chunkedEncodingIsCurrentlyLength = true;
                chunkedEncodingLastLength = 0;
                chunkedEncodingRemainingBytesForConsume = 0;
                headerSectionLength = 0;
                headerCount = 0;
                trailerSectionLength = 0;
                trailerCount = 0;
                leftoverBytes.clear();
                response.reset();
            }

        private:
            struct ChunkExtensionView{
                std::string_view name;
                std::string_view rawValue;
                bool hasValue;
                bool quoted;
            };

            constexpr void processChunkExtension(ChunkExtensionView extension) noexcept{
                (void)extension;
            }

            constexpr std::expected<void, httpParseError>
            parseChunkExtensions(std::string_view extensions)
            {
                if(extensions.size() > limits::MaxChunkExtensionsLength)
                    return lengthLimitError("Chunk extensions exceed configured maximum length");

                auto invalidExtension = [extensions]{
                    return std::unexpected{httpParseError{
                        .type = httpParseErrorType::UnrecognizedToken,
                        .parseContextText = std::string(extensions),
                        .what = "Invalid chunk extension syntax"
                    }};
                };

                std::size_t position = 0;
                while(position < extensions.size()){
                    while(position < extensions.size() &&
                        utils::isChunkExtensionWhitespace(extensions[position]))
                        ++position;

                    if(position == extensions.size() || extensions[position] != ';')
                        return invalidExtension();

                    ++position;
                    while(position < extensions.size() &&
                        utils::isChunkExtensionWhitespace(extensions[position]))
                        ++position;

                    const auto nameStart = position;
                    while(position < extensions.size() && utils::isTChar(extensions[position]))
                        ++position;

                    if(position == nameStart)
                        return invalidExtension();

                    const auto name = extensions.substr(nameStart, position - nameStart);
                    const auto nameEnd = position;

                    while(position < extensions.size() &&
                        utils::isChunkExtensionWhitespace(extensions[position]))
                        ++position;

                    bool hasValue = false;
                    bool quoted = false;
                    std::string_view value;

                    if(position < extensions.size() && extensions[position] == '='){
                        hasValue = true;
                        ++position;

                        while(position < extensions.size() &&
                            utils::isChunkExtensionWhitespace(extensions[position]))
                            ++position;

                        if(position == extensions.size())
                            return invalidExtension();

                        if(extensions[position] == '"'){
                            quoted = true;
                            const auto valueStart = ++position;

                            while(position < extensions.size() && extensions[position] != '"'){
                                if(extensions[position] == '\\'){
                                    ++position;
                                    if(position == extensions.size() ||
                                        !utils::isQuotedPairValue(extensions[position]))
                                        return invalidExtension();
                                } else if(!utils::isQuotedText(extensions[position])){
                                    return invalidExtension();
                                }

                                ++position;
                            }

                            if(position == extensions.size())
                                return invalidExtension();

                            value = extensions.substr(valueStart, position - valueStart);
                            ++position;
                        } else{
                            const auto valueStart = position;
                            while(position < extensions.size() && utils::isTChar(extensions[position]))
                                ++position;

                            if(position == valueStart)
                                return invalidExtension();

                            value = extensions.substr(valueStart, position - valueStart);
                        }
                    } else if(position == extensions.size() && position != nameEnd){
                        return invalidExtension();
                    }

                    processChunkExtension(ChunkExtensionView{
                        .name = name,
                        .rawValue = value,
                        .hasValue = hasValue,
                        .quoted = quoted
                    });
                }

                return {};
            }

            constexpr std::expected<void, httpParseError>
            parseTransferEncoding(std::string_view value) const
            {
                auto unsupported = [value]{
                    return std::unexpected{httpParseError{
                        .type = httpParseErrorType::UnimplementedFeature,
                        .parseContextText = std::string(value),
                        .what = "Only a single chunked transfer coding is currently supported"
                    }};
                };

                std::size_t position = 0;
                while(position < value.size() && (value[position] == ' ' || value[position] == '\t'))
                    ++position;

                const auto codingStart = position;
                while(position < value.size() && utils::isTChar(value[position]))
                    ++position;

                if(!utils::asciiCaseInsensitiveEquals(
                    value.substr(codingStart, position - codingStart), "chunked"))
                    return unsupported();

                while(position < value.size() && (value[position] == ' ' || value[position] == '\t'))
                    ++position;

                if(position != value.size())
                    return unsupported();

                return {};
            }

            std::expected<HeaderField, httpParseError>
            getHeader(std::string::size_type currentHeaderEnd)
            {
                const auto message = std::string_view{constructed};
                const auto colon = message.find(':', lastProcessedIdx);

                if(colon == std::string_view::npos || colon >= currentHeaderEnd)
                    return std::unexpected{httpParseError{
                        .type = httpParseErrorType::ExpectedMissingToken,
                        .parseContextText = contextLine(lastProcessedIdx, currentHeaderEnd),
                        .what = "Missing colon delimiter for current header"
                    }};

                const auto name = message.substr(lastProcessedIdx, colon - lastProcessedIdx);
                if(name.empty())
                    return std::unexpected{httpParseError{
                        .type = httpParseErrorType::InvalidHeaderFormat,
                        .parseContextText = contextLine(lastProcessedIdx, currentHeaderEnd),
                        .what = "Header name cannot be empty"
                    }};

                if(name.size() > limits::MaxHeaderNameLength)
                    return lengthLimitError("Header field name exceeds configured maximum length");

                for(const char c : name){
                    if(!utils::isTChar(c))
                        return std::unexpected{httpParseError{
                            .type = httpParseErrorType::DisallowedTokenChar,
                            .parseContextText = contextLine(lastProcessedIdx, currentHeaderEnd),
                            .what = "Header name contains a prohibited character"
                        }};
                }

                auto valueStart = colon + 1;
                while(valueStart < currentHeaderEnd &&
                    (message[valueStart] == ' ' || message[valueStart] == '\t'))
                    ++valueStart;

                auto valueEnd = currentHeaderEnd;
                while(valueEnd > valueStart &&
                    (message[valueEnd - 1] == ' ' || message[valueEnd - 1] == '\t'))
                    --valueEnd;

                if(valueEnd - valueStart > limits::MaxHeaderValueLength)
                    return lengthLimitError("Header field value exceeds configured maximum length");

                for(auto idx = valueStart; idx < valueEnd; ++idx){
                    const auto byte = static_cast<unsigned char>(message[idx]);
                    if(message[idx] != '\t' && (byte < 0x20 || byte == 0x7f))
                        return std::unexpected{httpParseError{
                            .type = httpParseErrorType::InvalidHeaderFormat,
                            .parseContextText = contextLine(lastProcessedIdx, currentHeaderEnd),
                            .what = "Header value contains a prohibited control character"
                        }};
                }

                HeaderField header{
                    .name = std::string{name},
                    .value = std::string{message.substr(valueStart, valueEnd - valueStart)}
                };

                lastProcessedIdx = currentHeaderEnd + 2;
                return header;
            }

            [[nodiscard]] Response::Headers::iterator findHeader(std::string_view name) noexcept{
                return std::find_if(
                    response.headers.begin(),
                    response.headers.end(),
                    [name](const auto& header){ return header.nameEquals(name); });
            }

            [[nodiscard]] bool hasHeader(std::string_view name) const noexcept{
                return std::any_of(
                    response.headers.begin(),
                    response.headers.end(),
                    [name](const auto& header){ return header.nameEquals(name); });
            }

            std::expected<httpParseStatus, httpParseError> finishWithLeftovers(){
                leftoverBytes.append(std::string_view{constructed}.substr(lastProcessedIdx));
                state = Processing::Finished;
                return httpParseStatus::Done;
            }

            std::unexpected<httpParseError> malformedStatusLine(std::string_view line) const{
                return std::unexpected{httpParseError{
                    .type = httpParseErrorType::UnrecognizedToken,
                    .parseContextText = std::string(line),
                    .what = "Malformed HTTP response status line"
                }};
            }

            std::unexpected<httpParseError> lengthLimitError(std::string message) const{
                return std::unexpected{httpParseError{
                    .type = httpParseErrorType::InvalidLength,
                    .parseContextText = contextFrom(lastProcessedIdx),
                    .what = std::move(message)
                }};
            }

            std::string contextFrom(std::string::size_type start) const{
                if(start == std::string::npos || start >= constructed.size())
                    return constructed;

                auto end = constructed.find("\r\n", start);
                if(end == std::string::npos)
                    end = constructed.size();

                return constructed.substr(start, end - start);
            }

            std::string contextLine(
                std::string::size_type start,
                std::string::size_type end) const
            {
                if(start == std::string::npos || start >= constructed.size())
                    return {};

                if(end == std::string::npos || end > constructed.size())
                    end = constructed.size();

                if(end < start)
                    return {};

                return constructed.substr(start, end - start);
            }

            std::string constructed;
            std::string::size_type lastProcessedIdx = 0;
            Response response;
            std::size_t bodySize = 0;
            std::size_t remaining = 0;
            Processing state = Processing::StatusLine;
            BodyFraming bodyFramingType = BodyFraming::None;

            bool chunkedEncodingIsCurrentlyLength = true;
            std::size_t chunkedEncodingLastLength = 0;
            std::size_t chunkedEncodingRemainingBytesForConsume = 0;

            std::size_t headerSectionLength = 0;
            std::size_t headerCount = 0;
            std::size_t trailerSectionLength = 0;
            std::size_t trailerCount = 0;

            std::string leftoverBytes;
            bool responseToHead = false;
            bool responseToConnect = false;
    };
} // namespace ninttp::internal
