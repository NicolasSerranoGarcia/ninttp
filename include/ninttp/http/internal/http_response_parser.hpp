#pragma once

#include <cassert>
#include <cstddef>
#include <expected>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

#include "../http_limits.hpp"
#include "../types.hpp"
#include "http_parse_error.hpp"

namespace ninttp::internal
{

    //uses builder pattern to craft a Request object that can be retrieved when a packet is completed
    template<httpVersion ver = http_1_0>
    class httpResponseParser{

        enum class Processing{
            StatusLine,
            Headers,
            Body,
            Finished
        };

        public:
            //TODO: we should most definitely delegate to a specific parser depending on the method read. 
            //most certainly synced with a stream socket connection
            //we can reuse this structure because the parser is agnostic of the body of the message. It only generates a Request
            //message, which then can get passed to a RequestHandler for example that 
            std::expected<httpParseStatus, httpParseError> append(const std::string& received){
                assert(state != Processing::Finished);
                constructed.append(received);

                while(true){
                    switch(state){
                        case Processing::StatusLine:{
                            std::istringstream ss(constructed);
                            std::size_t lineEnd;
                            if(lineEnd = constructed.find("\r\n"); lineEnd == std::string::npos){
                                if(constructed.size() > limits::MaxStatusLineLength)
                                    return lengthLimitError("Status line exceeds configured maximum length");

                                return httpParseStatus::NeedData;
                            }

                            if(lineEnd + 2 > limits::MaxStatusLineLength)
                                return lengthLimitError("Status line exceeds configured maximum length");

                            std::string version;
                            ss >> version >> response.statusCode;

                            if(!ss)
                                return std::unexpected{httpParseError{ .type = httpParseErrorType::UnrecognizedToken,
                                                                        .parseContextText = contextLine(0, lineEnd),
                                                                        .what = "Malformed status line"}};

                            if(auto v = httpVersion::fromRequestLineVersion(version); !v.has_value() || v.value().major != ver.major)
                                return std::unexpected{httpParseError{ .type = httpParseErrorType::UnsupportedVersion,
                                                                        .parseContextText = contextLine(0, lineEnd),
                                                                        .what = std::string("Cannot parse version ") + version +
                                                                            std::string(" with the specified version ") + ver.toString()}};
                            else
                                response.version = v.value();

                            if(response.statusCode < 100 || response.statusCode > 999)
                                return std::unexpected{httpParseError{ .type = httpParseErrorType::UnrecognizedToken,
                                                                        .parseContextText = contextLine(0, lineEnd),
                                                                        .what = std::string("Unrecognized status code: ") + std::to_string(response.statusCode)}};

                            lastProcessedIdx = lineEnd;
                            assert(lastProcessedIdx != std::string::npos);

                            std::clog << "[http.response_parser] state=StatusLine -> Headers\n";

                            state = Processing::Headers;
                            break;
                        }

                        case Processing::Headers:{
                            std::size_t headerEnd;

                            std::clog << "[http.response_parser] state=Headers; searching header terminator\n";
                            if(headerEnd = constructed.find("\r\n\r\n", lastProcessedIdx); headerEnd == std::string::npos){
                                if(constructed.size() - static_cast<std::size_t>(lastProcessedIdx) >
                                    limits::MaxHeaderSectionLength)
                                    return lengthLimitError("Header section exceeds configured maximum length");

                                std::clog << "[http.response_parser] state=Headers; did not find CRLFCRLF for headers\n";
                                return httpParseStatus::NeedData;
                            }

                            if(headerEnd - static_cast<std::size_t>(lastProcessedIdx) + 4 >
                                limits::MaxHeaderSectionLength)
                                return lengthLimitError("Header section exceeds configured maximum length");

                            //TODO: for both request and resonse parsers
                            //No headers does not necessarily mean no body? at least for the response it is strictily not needed because streams until shutdown of 
                            //transmissions are also accepted as responses
                            //no headers
                            if(lastProcessedIdx == headerEnd){
                                lastProcessedIdx += 4;
                                state = Processing::Finished;
                                return httpParseStatus::Done;
                            }

                            lastProcessedIdx += 2;
                            std::size_t headerCount = 0;

                            while(lastProcessedIdx < headerEnd){
                                std::size_t lineEnd;

                                if(lineEnd = constructed.find("\r\n", lastProcessedIdx); lineEnd == std::string::npos)
                                    return httpParseStatus::NeedData;

                                if(lineEnd - static_cast<std::size_t>(lastProcessedIdx) >
                                    limits::MaxHeaderLineLength)
                                    return lengthLimitError("Header line exceeds configured maximum length");

                                if(++headerCount > limits::MaxHeaderCount)
                                    return lengthLimitError("Header count exceeds configured maximum");

                                auto colon = constructed.find(':', lastProcessedIdx);

                                if(colon == std::string::npos || colon >= lineEnd)
                                    return std::unexpected{httpParseError{ .type = httpParseErrorType::ExpectedMissingToken,
                                                                            .parseContextText = contextLine(lastProcessedIdx, lineEnd),
                                                                            .what = "Malformed http packet"}};

                                if(colon - static_cast<std::size_t>(lastProcessedIdx) >
                                    limits::MaxHeaderNameLength)
                                    return lengthLimitError("Header field name exceeds configured maximum length");

                                const auto rawValueLength = lineEnd - colon - 1;
                                if(rawValueLength > limits::MaxHeaderValueLength)
                                    return lengthLimitError("Header field value exceeds configured maximum length");

                                internal::HeaderField header;

                                while(lastProcessedIdx != colon){
                                    header.name += constructed[lastProcessedIdx];
                                    lastProcessedIdx++;
                                }

                                assert(lastProcessedIdx < lineEnd);

                                if(constructed[lastProcessedIdx+1] == ' ')
                                    lastProcessedIdx++;

                                while(lastProcessedIdx != lineEnd){
                                    header.value += constructed[lastProcessedIdx];
                                    lastProcessedIdx++;
                                }

                                lastProcessedIdx += 2;

                                response.headers.push_back(std::move(header));
                            }

                            for(const auto& header : response.headers){
                                if(header.name != std::string("Content-Length"))
                                    continue;

                                try{
                                    bodySize = std::stoi(header.value);
                                } catch(std::invalid_argument& inv){
                                    return std::unexpected{
                                        httpParseError{ .type = httpParseErrorType::UnrecognizedToken,
                                                        .parseContextText = header.value,
                                                        .what = std::string{"Expected valid Content-Length, given "} + header.value }
                                    };
                                } catch(std::out_of_range& r){
                                    return std::unexpected{
                                        httpParseError{ .type = httpParseErrorType::InvalidLength,
                                                        .parseContextText = header.value,
                                                        .what = std::string{"Content-Length field exceeds allowed range"} }
                                    };
                                }

                                if(bodySize < 0){
                                    return std::unexpected{
                                        httpParseError{ .type = httpParseErrorType::InvalidLength,
                                                        .parseContextText = header.value,
                                                        .what = std::string{"Expected Content-Length field to be non-negative, given "} + header.value }
                                    };
                                }

                                if(static_cast<std::size_t>(bodySize) > limits::MaxBodyLength)
                                    return lengthLimitError("Content-Length exceeds configured maximum body length");

                                break;
                            }

                            //no body so the message is complete
                            if(bodySize == 0){
                                state = Processing::Finished;
                                return httpParseStatus::Done;
                            }

                            assert(lastProcessedIdx-2 == headerEnd);

                            lastProcessedIdx += 2;

                            std::clog << "[http.response_parser] state=Headers -> Body; body_size=" << bodySize << '\n';

                            state = Processing::Body;

                            break;
                        }

                        case Processing::Body:{
                            if(constructed.size() - lastProcessedIdx < bodySize)
                                return httpParseStatus::NeedData;

                            response.body.emplace();

                            for(std::ptrdiff_t i = 0; i != bodySize; i++)
                                response.body->push_back(constructed[lastProcessedIdx + i]);

                            std::clog << "[http.response_parser] state=Body -> Finished\n";

                            state = Processing::Finished;

                            return httpParseStatus::Done;
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

            bool processingStatusLine() const noexcept{
                return state == Processing::StatusLine;
            }

            Response getResponse() noexcept{
                assert(state == Processing::Finished);
                reset();
                return std::move(response); //TODO: check the move constructor of Response and assert it leaves it as the default constructor (invalid)
            }

        private:

            void reset() noexcept{
                constructed.clear();
                lastProcessedIdx = -1;
                bodySize = -1;
                state = Processing::StatusLine;
            }

            std::unexpected<httpParseError> lengthLimitError(std::string message) const{
                const auto start = lastProcessedIdx < 0
                    ? std::string::size_type{0}
                    : static_cast<std::string::size_type>(lastProcessedIdx);

                return std::unexpected{httpParseError{ .type = httpParseErrorType::InvalidLength,
                                                        .parseContextText = contextLine(start, constructed.size()),
                                                        .what = std::move(message)}};
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
            std::ptrdiff_t lastProcessedIdx = -1;
            //synced with the constructed string as new info gets received
            Response response;
            std::ptrdiff_t bodySize = -1; // maybe use something bigger in the future?
            Processing state = Processing::StatusLine;
    };
} // namespace ninttp::internal
