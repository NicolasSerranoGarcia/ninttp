#pragma once

#include <string>
#include <cstddef>
#include <sstream>
#include <optional>
#include <expected>
#include <cassert>
#include <iostream>

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

                while(1){
                    switch(state){
                        case Processing::StatusLine:{
                            std::istringstream ss(constructed);
                            size_t lineEnd;
                            if(lineEnd = constructed.find("\r\n"); lineEnd == std::string::npos)
                                return httpParseStatus::NeedData;

                            std::string version;
                            ss >> version >> response.statusCode;

                            if(!ss)
                                return std::unexpected{httpParseError{std::string("Malformed status line")}};

                            if(auto v = httpVersion::fromRequestLine(version); !v.has_value() || v.value().major != ver.major)
                                return std::unexpected{httpParseError{std::string("Cannot parse version ") + version + 
                                                        std::string(" with the specified version ") + ver.toString()}};
                            else
                                response.version = v.value();

                            if(response.statusCode < 100 || response.statusCode > 999)
                                return std::unexpected{httpParseError{std::string("Unrecognized status code: ") + std::to_string(response.statusCode)}};

                            lastProcessedIdx = lineEnd;
                            assert(lastProcessedIdx != std::string::npos);

                            std::clog << "[http.response_parser] state=StatusLine -> Headers\n";

                            state = Processing::Headers;
                            break;
                        }

                        case Processing::Headers:{
                            size_t headerEnd;

                            std::clog << "[http.response_parser] state=Headers; searching header terminator\n";
                            if(headerEnd = constructed.find("\r\n\r\n", lastProcessedIdx); headerEnd == std::string::npos){
                                std::clog << "[http.response_parser] state=Headers; did not find CRLFCRLF for headers\n";
                                return httpParseStatus::NeedData;
                            }

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

                            while(lastProcessedIdx < headerEnd){
                                size_t lineEnd;

                                if(lineEnd = constructed.find("\r\n", lastProcessedIdx); lineEnd == std::string::npos)
                                    return httpParseStatus::NeedData;

                                auto colon = constructed.find(':', lastProcessedIdx);

                                if(colon == std::string::npos)
                                    return std::unexpected{httpParseError{ .what = "Malformed http packet"}};

                                internal::Header header;

                                while(lastProcessedIdx != colon){
                                    header.key += constructed[lastProcessedIdx];
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

                            for(const auto& header : request.headers){
                                if(header.key != std::string("Content-Length"))
                                    continue;

                                try{
                                    bodySize = std::stoi(header.value);
                                } catch(std::invalid_argument& inv){
                                    return std::unexpected{
                                        httpParseError{ .what = std::string{"Expected valid Content-Length, given "} + header.value}
                                    };
                                } catch(std::out_of_range& r){
                                    return std::unexpected{
                                        httpParseError{ .what = "Content-Length field exceeds allowed range"};
                                    };
                                }

                                break;
                            }

                            if(bodySize < 0){
                                return std::unexpected{
                                    httpParseError{ .what = std::string{"Expected Content-Length field to be non-negative, given "} + header.value}
                                };
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

                            for(int i = 0; i != bodySize; i++)
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

            std::string constructed;
            std::ptrdiff_t lastProcessedIdx = -1;
            //synced with the constructed string as new info gets received
            Response response;
            std::ptrdiff_t bodySize = -1; // maybe use something bigger in the future?
            Processing state = Processing::StatusLine;
    };
} // namespace ninttp::internal
