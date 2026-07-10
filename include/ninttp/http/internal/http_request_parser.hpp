#pragma once

#include <cassert>
#include <charconv>
#include <cstddef>
#include <expected>
#include <iostream>
#include <optional>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

#include "http_parse_error.hpp"
#include "../types.hpp"
#include "parse_utils.hpp"

using namespace ninttp::utils;

//TODO: move all static private utilities to a parse_utilities.hpp header file so that they can be reused in the response parser and leaves bloat off this file

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
            Finished
        };

        enum class RequestLineProcessing{
            Method,
            Target,
            Version
        };

        public:
            std::expected<httpParseStatus, httpParseError> append(const std::string& received){
                assert(state != Processing::Finished);
                constructed.append(received);

                while(1){
                    switch(state){
                        case Processing::RequestLine:{
                            if(constructed.size() > MaxRequestLineLength)
                                return std::unexpected{httpParseError{ .type = httpParseErrorType::RequestLineTooLong,
                                                                        .parseContextText = contextFrom(lastProcessedIdx == std::string::npos ? 0 : lastProcessedIdx),
                                                                        .what = "Request line exceeds total length allowed by the RFC 9112"}};

                            switch(requestLineState){
                                case RequestLineProcessing::Method:{
                                    assert(lastProcessedIdx == std::string::npos);

                                    if(hasPrecedingWhitespace(constructed))
                                        return std::unexpected{httpParseError{ .type = httpParseErrorType::ExtraWhitespace,
                                                                                .parseContextText = contextFrom(0),
                                                                                .what = "Request line contains preceeding whitespace"}};

                                    //SYNC POINT: space delimited between the method and the resource
                                    auto methodResourceSP = constructed.find(' ');

                                    if(methodResourceSP == std::string::npos){
                                        if(constructed.size() > MaxMethodLength)
                                            return std::unexpected{httpParseError{ .type = httpParseErrorType::MethodTooLong, 
                                                                                .parseContextText = contextFrom(0),
                                                                                .what = "Method length exceeds max length possible for a standard method"}};

                                        return httpParseStatus::NeedData;
                                    }

                                    std::string_view lineMethod = std::string_view{constructed}.substr(0, methodResourceSP);

                                    for(const auto m : allHttpMethods){
                                        if(allHttpMethodsStr[static_cast<const int>(m)] == lineMethod)
                                            request.method = m;
                                    }

                                    if(request.method == httpMethod::INVALID)
                                        return std::unexpected{httpParseError{ .type = httpParseErrorType::UnrecognizedToken,
                                                                                .parseContextText = std::string(lineMethod),
                                                                                .what = std::string("Unrecognized method: ") + std::string(lineMethod)}};

                                    lastProcessedIdx = methodResourceSP+1;
                                    requestLineState = RequestLineProcessing::Target;
                                    break;
                                }

                                case RequestLineProcessing::Target:{

                                    if(hasPrecedingWhitespace(std::string_view{constructed}.substr(lastProcessedIdx)))
                                        return std::unexpected{httpParseError{ .type = httpParseErrorType::ExtraWhitespace,
                                                                                .parseContextText = contextFrom(lastProcessedIdx),
                                                                                .what = "Request line contains extra whitespace between method and target"}};

                                    if(std::string_view{constructed}.substr(lastProcessedIdx).size() > MaxRequestTargetLength)
                                        return std::unexpected{httpParseError{ .type = httpParseErrorType::TargetTooLong, 
                                                                            .parseContextText = contextFrom(lastProcessedIdx),
                                                                            .what = "Target length exceeds max length possible for RFC 9112"}};

                                    //SYNC POINT: space delimited between the resource and the version
                                    auto targetVersionSP = constructed.find(' ', lastProcessedIdx);
                                    auto requestLineEnd = constructed.find("\r\n", lastProcessedIdx);

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

                                    request.target = constructed.substr(lastProcessedIdx, targetVersionSP - lastProcessedIdx);

                                    //TODO: validate syntactic correctness of request.target only to comply with standard requirements of RFC

                                    lastProcessedIdx = targetVersionSP+1;
                                    requestLineState = RequestLineProcessing::Version;
                                    break;
                                }

                                case RequestLineProcessing::Version:{

                                    if(hasPrecedingWhitespace(std::string_view{constructed}.substr(lastProcessedIdx)))
                                        return std::unexpected{httpParseError{ .type = httpParseErrorType::ExtraWhitespace,
                                                                                .parseContextText = contextFrom(lastProcessedIdx),
                                                                                .what = "Request line contains extra whitespace between target and version"}};

                                    //SYNC POINT: CRLF delimiting request line
                                    std::string::size_type requestLineEnd = constructed.find("\r\n", lastProcessedIdx);

                                    std::string::size_type versionLength = requestLineEnd == std::string::npos ? requestLineEnd : requestLineEnd - lastProcessedIdx;

                                    //search until the end of constructed in case we havent found CRLF, if not until CRLF to avoid including parts of header
                                    if(std::string_view{constructed}.substr(lastProcessedIdx, versionLength).size() > HTTPVersionLength)
                                        return std::unexpected{httpParseError{ .type = httpParseErrorType::VersionTooLong,
                                                                                .parseContextText = contextFrom(lastProcessedIdx),
                                                                                .what = "Version length exceeds expected length of 8 bytes (HTTP/X.X)"}};

                                    if(requestLineEnd == std::string::npos)
                                        return httpParseStatus::NeedData;

                                    std::string_view version = std::string_view{constructed}.substr(lastProcessedIdx, requestLineEnd - lastProcessedIdx);

                                    if(hasTrailingWhitespace(version))
                                        return std::unexpected{httpParseError{ .type = httpParseErrorType::ExtraWhitespace,
                                                                                .parseContextText = std::string(version),
                                                                                .what = "Request line contains extra trailing whitespace"}};

                                    //about request lines: 
                                    //Request HTTP/1.0 to HTTP/1.1-capable server:
                                    //parse it, respond HTTP/1.1 or HTTP/1.0, depending on what the user specified on the httpServer template method.
                                    //even if you can respond with higher, the version template mandates what does the server respond with

                                    //Request HTTP/1.1 to HTTP/1.0-capable server:
                                    //same major version, higher minor. Should process as highest HTTP/1.x you support.

                                    //Request HTTP/2.0 on an HTTP/1.x text connection:
                                    //do not silently ignore. Either reject/close or send 505 if you can still produce an HTTP/1.x response.

                                    //Malformed request-line:
                                    //400 Bad Request, generally.

                                    //Valid request-line but unsupported major version:
                                    //505 HTTP Version Not Supported.

                                    auto v = httpVersion::fromRequestLineVersion(version);

                                    //the version value is malformed from the line itself, no logic involved
                                    if(!v.has_value())
                                        return std::unexpected{httpParseError{ .type = httpParseErrorType::UnrecognizedVersion, 
                                                                                .parseContextText = std::string(version),
                                                                                .what = std::string("Unrecognized version from request line ") + std::string(version)}};

                                    if(v.value().major > ver.major)
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
                            assert(request.method != httpMethod::INVALID);
                            size_t currentHeaderEnd;

                            while(1){
                                //SYNC POINT: CRLF delimiting one header at a time
                                if(currentHeaderEnd = constructed.find("\r\n", lastProcessedIdx); currentHeaderEnd == std::string::npos)
                                    return httpParseStatus::NeedData;

                                //there are no remaining headers
                                if(lastProcessedIdx == currentHeaderEnd){
                                    lastProcessedIdx += 2;
                                    break;
                                }

                                auto header = getHeader(currentHeaderEnd);

                                if(!header.has_value())
                                    return std::unexpected{std::move(header).error()};

                                auto parsedHeader = std::move(header).value();
                                parsedHeader.name = toLower(std::move(parsedHeader.name));

                                //TODO: normalize standard headers into an enum to avoid string comparisons
                                if((parsedHeader.name == "content-length" && request.headers.contains("transfer-encoding")) ||
                                    (parsedHeader.name == "transfer-encoding" && request.headers.contains("content-length")))
                                {
                                    return std::unexpected{httpParseError{ .type = httpParseErrorType::IncompatibleHeaders, 
                                                                            .parseContextText = parsedHeader.name + ": " + parsedHeader.value,
                                                                            .what = "Request contains both Content-Length and Transfer-Encoding headers"}};
                                } else if(parsedHeader.name == "content-length"){
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

                                    if(request.headers.contains(parsedHeader.name)){
                                        if(request.headers[parsedHeader.name] != parsedHeader.value)
                                            return std::unexpected{httpParseError{ .type = httpParseErrorType::DuplicatedHeader,
                                                                                    .parseContextText = parsedHeader.name + ": " + parsedHeader.value,
                                                                                    .what = "content-length header appears more than once with different values"}};

                                        continue;
                                    }
                                }

                                if(parsedHeader.name == "host" && request.headers.contains("host")){
                                    return std::unexpected{httpParseError{ .type = httpParseErrorType::DuplicatedHeader,
                                                                            .parseContextText = parsedHeader.name + ": " + parsedHeader.value,
                                                                            .what = "host header appears more than once"}};
                                }

                                if(request.headers.contains(parsedHeader.name)){
                                    request.headers[parsedHeader.name].append(", ");
                                    request.headers[parsedHeader.name].append(parsedHeader.value);
                                } else {
                                    request.headers[parsedHeader.name] = parsedHeader.value;
                                }
                            }

                            if(!request.headers.contains("host"))
                                return std::unexpected{httpParseError{ .type = httpParseErrorType::MissingHostHeader,
                                                                        .parseContextText = contextFrom(0),
                                                                        .what = "Expected request to contain required host header but did not find it"}};

                            if(bodySize == 0){
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
                state = Processing::RequestLine;
                requestLineState = RequestLineProcessing::Method;
                request.reset();
            }

        private:

            //dependent upon the state of constructed and lastProcessedIdx. This function just wraps the code that otherwise would be in the Header section of append,
            //so only groups code to make it clearer.
            //also advances lastProcessedIdx to the next header start
            constexpr std::expected<HeaderField, httpParseError> getHeader(std::string::size_type currentHeaderEnd){
                std::string_view headers = std::string_view{constructed};

                auto colon = headers.find(':', lastProcessedIdx);

                if(colon == std::string::npos)
                    //TODO: use the context atribute for aditional info, giving probably the whole header line
                    return std::unexpected{httpParseError{ .type = httpParseErrorType::ExpectedMissingToken, 
                                                            .parseContextText = contextLine(lastProcessedIdx, currentHeaderEnd),
                                                            .what = "Missing colon delimiter for current header"}};

                internal::HeaderField header{ .name = std::string{headers.substr(lastProcessedIdx, colon - lastProcessedIdx)}};

                if(header.name.empty())
                    return std::unexpected{httpParseError{ .type = httpParseErrorType::InvalidHeaderFormat,
                                        .parseContextText = contextLine(lastProcessedIdx, currentHeaderEnd),
                                        .what = "Header name contains prohibited chars specified in RFC 9112"}};

                lastProcessedIdx = colon+1;
                assert(lastProcessedIdx < currentHeaderEnd);

                if(constructed[lastProcessedIdx] == ' ')
                    lastProcessedIdx++;

                header.value = headers.substr(lastProcessedIdx, currentHeaderEnd - lastProcessedIdx);

                for(char c : header.name){
                    if(!isTChar(c))
                        return std::unexpected{httpParseError{ .type = httpParseErrorType::DisallowedTokenChar,
                                                                .parseContextText = header.name,
                                                                .what = "Header name contains prohibited chars specified in RFC 9112"}};
                }

                lastProcessedIdx = currentHeaderEnd + 2;

                return std::move(header);
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

            std::string leftoverBytes;

            static constexpr const std::size_t MaxMethodLength = 32;
            static constexpr const std::size_t MaxRequestTargetLength = 8000;
            static constexpr const std::size_t HTTPVersionLength = 8;
            static constexpr const std::size_t MaxRequestLineLength = MaxMethodLength + MaxRequestTargetLength + 4 /*2 SP & CRLF*/ + HTTPVersionLength;
    };
} // namespace ninttp::internal
