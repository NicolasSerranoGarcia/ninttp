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

#include "http_parse_error.hpp"
#include "../http_limits.hpp"
#include "../types.hpp"
#include "parse_utils.hpp"

//Possible optimizations: why not make every field of the request a view of the original buffer? Instead of copying parts of the buffer to the return object,
//we MOVE the constructed buffer to the request, as private member, and make getters or objects return views of the private object

namespace ninttp::internal
{

    //uses builder pattern to craft a Request object that can be retrieved when a message is completed
    //TODO: add extra state variable to allow retrieving the leftover bytes and the request without reseting the object.
    //this way we can reuse the object and expand its lifetime 
    template<httpVersion ver = http_1_0>
    class httpRequestParser{

        enum class Processing{
            RequestLine,
            Headers,
            Body,
            TrailingHeaders,
            Finished
        };

        enum class RequestLineProcessing{
            Method,
            Target,
            Version
        };
        
        enum class BodyFraming{
            None, 
            ContentLength,
            Chunked,
            ConnectionClose,
            Tunnel
        };

        public:
            std::expected<httpParseStatus, httpParseError> append(const std::string& received){
                assert(state != Processing::Finished);
                constructed.append(received);

                while(true){
                    switch(state){
                        case Processing::RequestLine:{
                            const auto requestLineEnd = constructed.find("\r\n");
                            const auto requestLineLength = requestLineEnd == std::string::npos
                                ? constructed.size()
                                : requestLineEnd + 2;

                            if(requestLineLength > limits::MaxRequestLineLength)
                                return std::unexpected{httpParseError{ .type = httpParseErrorType::RequestLineTooLong,
                                                                        .parseContextText = contextFrom(lastProcessedIdx == std::string::npos ? 0 : lastProcessedIdx),
                                                                        .what = "Request line exceeds total length allowed by the RFC 9112"}};

                            switch(requestLineState){
                                case RequestLineProcessing::Method:{
                                    assert(lastProcessedIdx == std::string::npos);

                                    if(utils::hasPrecedingWhitespace(constructed))
                                        return std::unexpected{httpParseError{ .type = httpParseErrorType::ExtraWhitespace,
                                                                                .parseContextText = contextFrom(0),
                                                                                .what = "Request line contains preceeding whitespace"}};

                                    //SYNC POINT: space delimited between the method and the resource
                                    auto methodResourceSP = constructed.find(' ');

                                    if(methodResourceSP == std::string::npos){
                                        if(constructed.size() > limits::MaxMethodLength)
                                            return std::unexpected{httpParseError{ .type = httpParseErrorType::MethodTooLong, 
                                                                                .parseContextText = contextFrom(0),
                                                                                .what = "Method token exceeds the configured maximum length"}};

                                        return httpParseStatus::NeedData;
                                    }

                                    std::string_view lineMethod = std::string_view{constructed}.substr(0, methodResourceSP);

                                    if(lineMethod.size() > limits::MaxMethodLength)
                                        return std::unexpected{httpParseError{ .type = httpParseErrorType::MethodTooLong,
                                                                                .parseContextText = std::string(lineMethod),
                                                                                .what = "Method token exceeds the configured maximum length"}};

                                    for(const char c : lineMethod){
                                        if(!utils::isTChar(c))
                                            return std::unexpected{httpParseError{ .type = httpParseErrorType::DisallowedTokenChar,
                                                                                    .parseContextText = std::string(lineMethod),
                                                                                    .what = "Method contains a character not allowed in an HTTP token"}};
                                    }

                                    request.method = lineMethod;

                                    lastProcessedIdx = methodResourceSP+1;
                                    requestLineState = RequestLineProcessing::Target;
                                    break;
                                }

                                case RequestLineProcessing::Target:{

                                    if(utils::hasPrecedingWhitespace(std::string_view{constructed}.substr(lastProcessedIdx)))
                                        return std::unexpected{httpParseError{ .type = httpParseErrorType::ExtraWhitespace,
                                                                                .parseContextText = contextFrom(lastProcessedIdx),
                                                                                .what = "Request line contains extra whitespace between method and target"}};

                                    //SYNC POINT: space delimited between the resource and the version
                                    auto targetVersionSP = constructed.find(' ', lastProcessedIdx);
                                    auto requestLineEnd = constructed.find("\r\n", lastProcessedIdx);
                                    const auto targetEnd =
                                        requestLineEnd != std::string::npos &&
                                        (targetVersionSP == std::string::npos || requestLineEnd < targetVersionSP)
                                            ? requestLineEnd
                                            : (targetVersionSP == std::string::npos ? constructed.size() : targetVersionSP);

                                    if(targetEnd - lastProcessedIdx > limits::MaxRequestTargetLength)
                                        return std::unexpected{httpParseError{ .type = httpParseErrorType::TargetTooLong,
                                                                            .parseContextText = contextFrom(lastProcessedIdx),
                                                                            .what = "Target length exceeds max length possible for RFC 9112"}};

                                    //avoid reporting the need for more data when we find a CRLF but not a SP, as CRLF is reserved only for the request line delimiter
                                    if(requestLineEnd != std::string::npos && (targetVersionSP == std::string::npos || requestLineEnd < targetVersionSP))
                                        return std::unexpected{httpParseError{ .type = httpParseErrorType::ExpectedMissingToken,
                                                                                .parseContextText = contextLine(lastProcessedIdx, requestLineEnd),
                                                                                .what = "Missing space delimiter between target and version"}};


                                    if(targetVersionSP == std::string::npos){
                                        //TODO: additionally maybe validate that target exists when constructing gradually?
                                        //here we would check existence of constructed against the stored resources, leaving early once 
                                        //we find a non registerd or other resource

                                        return httpParseStatus::NeedData;
                                    }

                                    const auto target = std::string_view{constructed}.substr(
                                        lastProcessedIdx,
                                        targetVersionSP - lastProcessedIdx);

                                    if(target.empty())
                                        return std::unexpected{httpParseError{ .type = httpParseErrorType::ExpectedMissingToken,
                                                                                .parseContextText = contextFrom(lastProcessedIdx),
                                                                                .what = "Request target cannot be empty"}};

                                    for(const char c : target){
                                        const auto byte = static_cast<unsigned char>(c);
                                        if(byte <= 0x20 || byte == 0x7f)
                                            return std::unexpected{httpParseError{ .type = httpParseErrorType::DisallowedTokenChar,
                                                                                    .parseContextText = std::string(target),
                                                                                    .what = "Request target contains whitespace or a control character"}};
                                    }

                                    request.target = target;

                                    lastProcessedIdx = targetVersionSP+1;
                                    requestLineState = RequestLineProcessing::Version;
                                    break;
                                }

                                case RequestLineProcessing::Version:{

                                    if(utils::hasPrecedingWhitespace(std::string_view{constructed}.substr(lastProcessedIdx)))
                                        return std::unexpected{httpParseError{ .type = httpParseErrorType::ExtraWhitespace,
                                                                                .parseContextText = contextFrom(lastProcessedIdx),
                                                                                .what = "Request line contains extra whitespace between target and version"}};

                                    //SYNC POINT: CRLF delimiting request line
                                    std::string::size_type requestLineEnd = constructed.find("\r\n", lastProcessedIdx);

                                    std::string::size_type versionLength = requestLineEnd == std::string::npos ? requestLineEnd : requestLineEnd - lastProcessedIdx;

                                    //search until the end of constructed in case we havent found CRLF, if not until CRLF to avoid including parts of header
                                    if(std::string_view{constructed}.substr(lastProcessedIdx, versionLength).size() > limits::HTTPVersionLength)
                                        return std::unexpected{httpParseError{ .type = httpParseErrorType::VersionTooLong,
                                                                                .parseContextText = contextFrom(lastProcessedIdx),
                                                                                .what = "Version length exceeds expected length of 8 bytes (HTTP/X.X)"}};

                                    if(requestLineEnd == std::string::npos)
                                        return httpParseStatus::NeedData;

                                    std::string_view version = std::string_view{constructed}.substr(lastProcessedIdx, requestLineEnd - lastProcessedIdx);

                                    if(utils::hasTrailingWhitespace(version))
                                        return std::unexpected{httpParseError{ .type = httpParseErrorType::ExtraWhitespace,
                                                                                .parseContextText = std::string(version),
                                                                                .what = "Request line contains extra trailing whitespace"}};

                                    auto v = httpVersion::fromRequestLineVersion(version);

                                    //the version value is malformed from the line itself, no logic involved
                                    if(!v.has_value())
                                        return std::unexpected{httpParseError{ .type = httpParseErrorType::UnrecognizedVersion, 
                                                                                .parseContextText = std::string(version),
                                                                                .what = std::string("Unrecognized version from request line ") + std::string(version)}};

                                    if(v.value().major != 1 || v.value().major > ver.major)
                                        return std::unexpected{httpParseError{ .type = httpParseErrorType::UnsupportedVersion, 
                                                                                .parseContextText = std::string(version),
                                                                                .what = std::string("Cannot parse request with version ") + std::string(version) + 
                                                                                std::string(" using the specified parser version ") + ver.toString()}};

                                    request.version = v.value();

                                    //we do sum +2 to the lastProcessedIdx bc the Headers must search for a double CRLF if there are no headers.
                                    //In the case there are no headers, we must search for this CRLF and the one that marks the header end.
                                    lastProcessedIdx = requestLineEnd+2;
                                    assert(lastProcessedIdx != std::string::npos);

                                    std::clog << "[http.request_parser] state=RequestLine -> Headers\n";

                                    state = Processing::Headers;
                                    break;
                                } // case RequestLineProcessing::Version
                            } // switch(state)
                            break;
                        } // case Processing::RequestLine

                        case Processing::Headers:{
                            assert(!request.method.empty());
                            std::size_t currentHeaderEnd;

                            while(true){
                                //SYNC POINT: CRLF delimiting one header at a time
                                if(currentHeaderEnd = constructed.find("\r\n", lastProcessedIdx); currentHeaderEnd == std::string::npos){
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

                                //there are no remaining headers
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

                                if((parsedHeader.name == "content-length" && request.headers.contains("transfer-encoding")) ||
                                    (parsedHeader.name == "transfer-encoding" && request.headers.contains("content-length")))
                                {
                                    return std::unexpected{httpParseError{ .type = httpParseErrorType::IncompatibleHeaders, 
                                                                            .parseContextText = parsedHeader.name + ": " + parsedHeader.value,
                                                                            .what = "Request contains both Content-Length and Transfer-Encoding headers"}};
                                } else if(parsedHeader.name == "content-length"){
                                    bodyFramingType = BodyFraming::ContentLength;
                                    request.bodyFraming = RequestBodyFraming::ContentLength;

                                    const char* first = parsedHeader.value.data();
                                    const char* last = first + parsedHeader.value.size();
                                    auto [ptr, ec] = std::from_chars(first, last, bodySize);

                                    if(ec == std::errc::invalid_argument || ptr != last)
                                        return std::unexpected{httpParseError{ .type = httpParseErrorType::UnrecognizedToken,
                                                                                .parseContextText = parsedHeader.value,
                                                                                .what = std::string("Expected valid Content-Length, given ") + parsedHeader.value}};

                                    if(ec == std::errc::result_out_of_range)
                                        return std::unexpected{httpParseError{ .type = httpParseErrorType::InvalidLength,
                                                                                .parseContextText = parsedHeader.value,
                                                                                .what = "Content-Length field exceeds allowed range"}};

                                    if(bodySize > limits::MaxBodyLength)
                                        return lengthLimitError("Content-Length exceeds configured maximum body length");

                                    if(request.headers.contains(parsedHeader.name)){
                                        if(request.headers[parsedHeader.name] != parsedHeader.value)
                                            return std::unexpected{httpParseError{ .type = httpParseErrorType::DuplicatedHeader,
                                                                                    .parseContextText = parsedHeader.name + ": " + parsedHeader.value,
                                                                                    .what = "content-length header appears more than once with different values"}};

                                        continue;
                                    }
                                }else if(parsedHeader.name == "transfer-encoding"){
                                    if(request.version.major != 1 || request.version.minor < 1)
                                        return std::unexpected{httpParseError{ .type = httpParseErrorType::UnexpectedToken,
                                                                                .parseContextText = parsedHeader.name + ": " + parsedHeader.value,
                                                                                .what = "Transfer-Encoding is not valid for an HTTP/1.0 request"}};

                                    if(request.headers.contains(parsedHeader.name))
                                        return std::unexpected{httpParseError{ .type = httpParseErrorType::DuplicatedHeader,
                                                                                .parseContextText = parsedHeader.name + ": " + parsedHeader.value,
                                                                                .what = "Transfer-Encoding header appears more than once"}};

                                    auto transferEncoding = parseTransferEncoding(parsedHeader.value);
                                    if(!transferEncoding.has_value())
                                        return std::unexpected{std::move(transferEncoding).error()};

                                    parsedHeader.value = "chunked";
                                    
                                    bodyFramingType = BodyFraming::Chunked;
                                    request.bodyFraming = RequestBodyFraming::Chunked;
                                }

                                if(parsedHeader.name == "host" && request.headers.contains("host"))
                                    return std::unexpected{httpParseError{ .type = httpParseErrorType::DuplicatedHeader,
                                                                            .parseContextText = parsedHeader.name + ": " + parsedHeader.value,
                                                                            .what = "host header appears more than once"}};

                                if(request.headers.contains(parsedHeader.name)){
                                    request.headers[parsedHeader.name].append(", ");
                                    request.headers[parsedHeader.name].append(parsedHeader.value);
                                } else {
                                    request.headers[parsedHeader.name] = parsedHeader.value;
                                }
                            }

                            if(request.version.major == 1 && request.version.minor >= 1 &&
                                !request.headers.contains("host"))
                                return std::unexpected{httpParseError{ .type = httpParseErrorType::MissingHostHeader,
                                                                        .parseContextText = contextFrom(0),
                                                                        .what = "Expected request to contain required host header but did not find it"}};

                            if(bodyFramingType != BodyFraming::Chunked && bodySize == 0){
                                leftoverBytes.append(std::string_view{constructed}.substr(lastProcessedIdx));
                                state = Processing::Finished;
                                return httpParseStatus::Done;
                            }

                            request.body.emplace();
                            remaining = bodySize;

                            std::clog << "[http.request_parser] state=Headers -> Body; body_size=" << bodySize << '\n';
                            state = Processing::Body;

                            break;
                        }

                        case Processing::Body:{

                            switch(bodyFramingType){
                                case BodyFraming::None:
                                    break;
                                case BodyFraming::ContentLength:{
                                    //we start at the next character of the CRLF that separates the headers and body

                                    auto arrived = std::string_view{constructed}.substr(lastProcessedIdx);

                                    if(arrived.size() < remaining){
                                        const auto consumed = arrived.size();
                                        request.body->append(arrived);
                                        remaining -= consumed;
                                        lastProcessedIdx += consumed;
                                        return httpParseStatus::NeedData;
                                    }

                                    const auto consumed = remaining;
                                    request.body->append(arrived.substr(0, consumed));
                                    leftoverBytes.append(arrived.substr(consumed));
                                    remaining = 0;

                                    lastProcessedIdx += consumed;

                                    std::clog << "[http.request_parser] state=Body -> Finished\n";

                                    state = Processing::Finished;
                                    return httpParseStatus::Done;

                                    break;
                                }
                                case BodyFraming::Chunked:{
                                    auto arrived = std::string_view{constructed}.substr(lastProcessedIdx);

                                    if(chunkedEncodingIsCurrentlyLength){
                                        std::string_view::size_type crlf;
                                        if(crlf = arrived.find("\r\n"); crlf == std::string_view::npos){
                                            if(arrived.size() > limits::MaxChunkLineLength)
                                                return lengthLimitError("Chunk-size line exceeds configured maximum length");

                                            return httpParseStatus::NeedData;
                                        }

                                        if(crlf > limits::MaxChunkLineLength)
                                            return lengthLimitError("Chunk-size line exceeds configured maximum length");

                                        auto chunkLine = arrived.substr(0, crlf);

                                        const char* first = chunkLine.data();
                                        const char* last = first + chunkLine.size();
                                        auto [ptr, ec] = std::from_chars(first, last, chunkedEncodingLastLength, 16);

                                        if(ec == std::errc::invalid_argument)
                                            return std::unexpected{httpParseError{ .type = httpParseErrorType::UnrecognizedToken,
                                                                                    .parseContextText = std::string(chunkLine),
                                                                                    .what = "Expected valid hexadecimal chunk length"}};

                                        if(ec == std::errc::result_out_of_range)
                                            return std::unexpected{httpParseError{ .type = httpParseErrorType::InvalidLength,
                                                                                    .parseContextText = std::string(chunkLine),
                                                                                    .what = "Chunk length exceeds allowed range"}};

                                        auto extensions = std::string_view{ptr, static_cast<std::size_t>(last - ptr)};
                                        auto parsedExtensions = parseChunkExtensions(extensions);

                                        if(!parsedExtensions.has_value())
                                            return std::unexpected{std::move(parsedExtensions).error()};

                                        if(chunkedEncodingLastLength > limits::MaxChunkLength)
                                            return lengthLimitError("Chunk length exceeds configured maximum");

                                        if(chunkedEncodingLastLength > limits::MaxBodyLength - request.body->size())
                                            return lengthLimitError("Decoded body exceeds configured maximum length");

                                        chunkedEncodingRemainingBytesForConsume = chunkedEncodingLastLength;

                                        if(chunkedEncodingLastLength == 0){
                                            state = Processing::TrailingHeaders;
                                        }

                                        lastProcessedIdx += crlf+2;
                                        chunkedEncodingIsCurrentlyLength = false;
                                    } else{

                                        if(chunkedEncodingRemainingBytesForConsume > arrived.size()){
                                            request.body->append(arrived.substr(0, arrived.size()));
                                            chunkedEncodingRemainingBytesForConsume -= arrived.size();
                                            lastProcessedIdx += arrived.size();
                                            return httpParseStatus::NeedData;
                                        }

                                        request.body->append(arrived.substr(0, chunkedEncodingRemainingBytesForConsume));
                                        lastProcessedIdx += chunkedEncodingRemainingBytesForConsume;
                                        auto remaining = arrived.substr(chunkedEncodingRemainingBytesForConsume);
                                        chunkedEncodingRemainingBytesForConsume = 0;

                                        std::string_view::size_type crlf;

                                        if(crlf = remaining.find("\r\n"); crlf == std::string_view::npos){
                                            return httpParseStatus::NeedData;
                                        }

                                        if(crlf != 0){
                                            return std::unexpected{httpParseError{ .type = httpParseErrorType::UnexpectedToken,
                                                                                    .parseContextText = std::string(remaining),
                                                                                    .what = "Expected CRLF to end chunk delimited by length, got more data"}};
                                        }

                                        lastProcessedIdx += 2;
                                        chunkedEncodingIsCurrentlyLength = true;
                                    }

                                    break;
                                }
                                case BodyFraming::ConnectionClose:
                                case BodyFraming::Tunnel:
                                default:
                                    break;
                            }
                            break;
                        }

                        case Processing::TrailingHeaders:{
                            std::size_t currentHeaderEnd;

                            while(true){
                                if(currentHeaderEnd = constructed.find("\r\n", lastProcessedIdx); currentHeaderEnd == std::string::npos){
                                    const auto pendingLength = constructed.size() - lastProcessedIdx;
                                    if(pendingLength > limits::MaxTrailerLineLength ||
                                        trailerSectionLength + pendingLength > limits::MaxTrailerSectionLength)
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
                                    leftoverBytes.append(std::string_view{constructed}.substr(lastProcessedIdx));
                                    state = Processing::Finished;
                                    return httpParseStatus::Done;
                                }

                                if(++trailerCount > limits::MaxTrailerCount)
                                    return lengthLimitError("Trailer field count exceeds configured maximum");

                                auto header = getHeader(currentHeaderEnd);

                                if(!header.has_value())
                                    return std::unexpected{std::move(header).error()};

                                auto parsedHeader = std::move(header).value();
                                parsedHeader.name = utils::toLower(std::move(parsedHeader.name));

                                if(!request.trailingHeaders.has_value())
                                    request.trailingHeaders.emplace();

                                request.trailingHeaders->push_back(std::move(parsedHeader));
                            }
                        }

                        default:
                            break;
                    }
                }

                return httpParseStatus::NeedData;
            }

            bool finished() const noexcept{
                return state == Processing::Finished;
            }

            bool processingBody() const noexcept{
                return state == Processing::Body;
            }

            bool processingHeaders() const noexcept{
                return state == Processing::Headers;
            }

            bool processingRequestLine() const noexcept{
                return state == Processing::RequestLine;
            }

            Request getRequest() noexcept{
                assert(state == Processing::Finished);
                return std::move(request);
            }

            bool hasLeftoverBytes() const noexcept{
                assert(state == Processing::Finished);
                return !leftoverBytes.empty();
            }

            //moves the leftover bytes from the last parsing into the caller
            std::string getLeftoverBytes() noexcept{
                assert(state == Processing::Finished);
                return std::move(leftoverBytes);
            }

            //correct workflow: getLeftOverBytes || getRequest and then reset. Calling reset on a parser with a ready request or leftover flushes the constructed result at any point.
            //use at your own risk
            constexpr void reset() noexcept{
                constructed.clear();
                lastProcessedIdx = std::string::npos;
                bodySize = 0;
                remaining = 0;
                state = Processing::RequestLine;
                requestLineState = RequestLineProcessing::Method;
                bodyFramingType = BodyFraming::None;
                chunkedEncodingIsCurrentlyLength = true;
                chunkedEncodingLastLength = 0;
                chunkedEncodingRemainingBytesForConsume = 0;
                headerSectionLength = 0;
                headerCount = 0;
                trailerSectionLength = 0;
                trailerCount = 0;
                leftoverBytes.clear();
                request.reset();
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

            constexpr std::expected<void, httpParseError> parseChunkExtensions(std::string_view extensions){
                if(extensions.size() > limits::MaxChunkExtensionsLength)
                    return std::unexpected{httpParseError{ .type = httpParseErrorType::InvalidLength,
                                                            .parseContextText = std::string(extensions),
                                                            .what = "Chunk extensions exceed configured maximum length"}};

                auto invalidExtension = [extensions]{
                    return std::unexpected{httpParseError{ .type = httpParseErrorType::UnrecognizedToken,
                                                            .parseContextText = std::string(extensions),
                                                            .what = "Invalid chunk extension syntax"}};
                };

                std::size_t position = 0;

                while(position < extensions.size()){
                    while(position < extensions.size() && utils::isChunkExtensionWhitespace(extensions[position]))
                        ++position;

                    if(position == extensions.size() || extensions[position] != ';')
                        return invalidExtension();

                    ++position;

                    while(position < extensions.size() && utils::isChunkExtensionWhitespace(extensions[position]))
                        ++position;

                    const auto nameStart = position;
                    while(position < extensions.size() && utils::isTChar(extensions[position]))
                        ++position;

                    if(position == nameStart)
                        return invalidExtension();

                    const auto name = extensions.substr(nameStart, position - nameStart);
                    const auto nameEnd = position;

                    while(position < extensions.size() && utils::isChunkExtensionWhitespace(extensions[position]))
                        ++position;

                    bool hasValue = false;
                    bool quoted = false;
                    std::string_view value;

                    if(position < extensions.size() && extensions[position] == '='){
                        hasValue = true;
                        ++position;

                        while(position < extensions.size() && utils::isChunkExtensionWhitespace(extensions[position]))
                            ++position;

                        if(position == extensions.size())
                            return invalidExtension();

                        if(extensions[position] == '"'){
                            quoted = true;
                            const auto valueStart = ++position;

                            while(position < extensions.size() && extensions[position] != '"'){
                                if(extensions[position] == '\\'){
                                    ++position;
                                    if(position == extensions.size() || !utils::isQuotedPairValue(extensions[position]))
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

            constexpr std::expected<void, httpParseError> parseTransferEncoding(std::string_view value) const{
                auto unsupported = [value]{
                    return std::unexpected{httpParseError{ .type = httpParseErrorType::UnimplementedFeature,
                                                            .parseContextText = std::string(value),
                                                            .what = "Only a single chunked transfer coding is currently supported"}};
                };

                std::size_t position = 0;
                while(position < value.size() && (value[position] == ' ' || value[position] == '\t'))
                    ++position;

                const auto codingStart = position;
                while(position < value.size() && utils::isTChar(value[position]))
                    ++position;

                const auto coding = value.substr(codingStart, position - codingStart);
                if(!utils::asciiCaseInsensitiveEquals(coding, "chunked"))
                    return unsupported();

                while(position < value.size() && (value[position] == ' ' || value[position] == '\t'))
                    ++position;

                if(position != value.size())
                    return unsupported();

                return {};
            }

            //dependent upon the state of constructed and lastProcessedIdx. This function just wraps the code that otherwise would be in the Header section of append,
            //so only groups code to make it clearer.
            //also advances lastProcessedIdx to the next header start
            constexpr std::expected<HeaderField, httpParseError> getHeader(std::string::size_type currentHeaderEnd){
                const auto headers = std::string_view{constructed};
                const auto colon = headers.find(':', lastProcessedIdx);

                if(colon == std::string_view::npos || colon >= currentHeaderEnd)
                    return std::unexpected{httpParseError{ .type = httpParseErrorType::ExpectedMissingToken, 
                                                            .parseContextText = contextLine(lastProcessedIdx, currentHeaderEnd),
                                                            .what = "Missing colon delimiter for current header"}};

                const auto name = headers.substr(lastProcessedIdx, colon - lastProcessedIdx);

                if(name.empty())
                    return std::unexpected{httpParseError{ .type = httpParseErrorType::InvalidHeaderFormat,
                                        .parseContextText = contextLine(lastProcessedIdx, currentHeaderEnd),
                                        .what = "Header name cannot be empty"}};

                if(name.size() > limits::MaxHeaderNameLength)
                    return lengthLimitError("Header field name exceeds configured maximum length");

                for(const char c : name){
                    if(!utils::isTChar(c))
                        return std::unexpected{httpParseError{ .type = httpParseErrorType::DisallowedTokenChar,
                                                                .parseContextText = contextLine(lastProcessedIdx, currentHeaderEnd),
                                                                .what = "Header name contains prohibited chars specified in RFC 9112"}};
                }

                auto valueStart = colon + 1;
                while(valueStart < currentHeaderEnd &&
                    (headers[valueStart] == ' ' || headers[valueStart] == '\t'))
                    ++valueStart;

                auto valueEnd = currentHeaderEnd;
                while(valueEnd > valueStart &&
                    (headers[valueEnd - 1] == ' ' || headers[valueEnd - 1] == '\t'))
                    --valueEnd;

                if(valueEnd - valueStart > limits::MaxHeaderValueLength)
                    return lengthLimitError("Header field value exceeds configured maximum length");

                for(auto idx = valueStart; idx < valueEnd; ++idx){
                    const auto byte = static_cast<unsigned char>(headers[idx]);
                    if(headers[idx] != '\t' && (byte < 0x20 || byte == 0x7f))
                        return std::unexpected{httpParseError{ .type = httpParseErrorType::InvalidHeaderFormat,
                                                                .parseContextText = contextLine(lastProcessedIdx, currentHeaderEnd),
                                                                .what = "Header value contains a prohibited control character"}};
                }

                internal::HeaderField header{
                    .name = std::string{name},
                    .value = std::string{headers.substr(valueStart, valueEnd - valueStart)}
                };

                lastProcessedIdx = currentHeaderEnd + 2;

                return header;
            }

            std::unexpected<httpParseError> lengthLimitError(std::string message) const{
                return std::unexpected{httpParseError{ .type = httpParseErrorType::InvalidLength,
                                                        .parseContextText = contextFrom(lastProcessedIdx),
                                                        .what = std::move(message)}};
            }

            std::string contextFrom(std::string::size_type start) const{
                if(start == std::string::npos || start >= constructed.size())
                    return constructed;

                auto end = constructed.find("\r\n", start);
                if(end == std::string::npos)
                    end = constructed.size();

                return constructed.substr(start, end - start);
            }

            std::string contextLine(std::string::size_type start, std::string::size_type end) const{
                if(start == std::string::npos || start >= constructed.size())
                    return {};

                if(end == std::string::npos || end > constructed.size())
                    end = constructed.size();

                if(end < start)
                    return {};

                return constructed.substr(start, end - start);
            }

            std::string constructed;
            std::string::size_type lastProcessedIdx = std::string::npos;
            //synced with the constructed string as new info gets received

            //TODO: assert this has a move constructor that leaves the object in it's original state, or at least a clear method
            Request request;
            std::size_t bodySize = 0;
            std::size_t remaining;
            Processing state = Processing::RequestLine;
            RequestLineProcessing requestLineState = RequestLineProcessing::Method;
            BodyFraming bodyFramingType = BodyFraming::None;

            //transfer encoding delimits stream by first sending a length, followed by a crlf and then the inmediate contents delimited by that ammount
            bool chunkedEncodingIsCurrentlyLength = true;
            std::size_t chunkedEncodingLastLength = 0;
            std::size_t chunkedEncodingRemainingBytesForConsume = 0;
            std::size_t headerSectionLength = 0;
            std::size_t headerCount = 0;
            std::size_t trailerSectionLength = 0;
            std::size_t trailerCount = 0;

            std::string leftoverBytes;

    };
} // namespace ninttp::internal
