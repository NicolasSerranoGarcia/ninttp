#pragma once

#include <string>
#include <cstddef>
#include <sstream>
#include <stdexcept>
#include <optional>
#include <expected>
#include <cassert>
#include <iostream>
#include <span>
#include <string_view>

#include "http_parse_error.hpp"
#include "../types.hpp"

namespace ninttp::internal
{

    //uses builder pattern to craft a Request object that can be retrieved when a packet is completed
    template<httpVersion ver = http_1_0>
    class httpRequestParser{

        enum class Processing{
            RequestLine,
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
                        //first time parsing. Store until we find CRLF and then process once
                        case Processing::RequestLine:{
                            std::string::size_type lineEnd;
                            //even if we cant find the CRLF (given that we still havent got to it), we could also check for identifiers of other parts of the
                            //message (like headers or other parts of the body), to lookahead and report that the CRLF is missing, and the parser is expecting
                            //it to continue, even though it seems like the message is "complete"? kind of what compilers do to report not just the first error
                            //they find, but skip it and find more errors downstream
                            if(lineEnd = constructed.find("\r\n"); lineEnd == std::string::npos)
                                return httpParseStatus::NeedData;

                            std::string_view requestLine(constructed.c_str(), lineEnd);

                            assert(lastProcessedIdx == std::string::npos);

                            if(hasPrecedingWhitespace(requestLine))
                                return std::unexpected{httpParseError{ .type = httpParseErrorType::ExtraWhitespace,
                                                                        .what = "Request line contains preceeding whitespace"}};

                            
                            auto sp = requestLine.find(' ');

                            if(sp == std::string::npos)
                                return std::unexpected{httpParseError{ .type = httpParseErrorType::ExpectedMissingToken, 
                                                                        .what = "Missing space delimiter between method and resource"}};
                    
                            std::string_view lineMethod = requestLine.substr(0, sp);
                            //+1 to skip the space
                            lastProcessedIdx = sp+1;

                            for(const auto m : all_http_methods){
                                if(std::string(httpVerbStr[static_cast<const int>(m)]) == lineMethod)
                                    request.method = m;
                            }

                            if(request.method == httpMethod::INVALID)
                                return std::unexpected{httpParseError{std::string("Unrecognized method: ") + std::string(lineMethod)}};

                            if(hasPrecedingWhitespace(requestLine.substr(lastProcessedIdx)))
                                return std::unexpected{httpParseError{ .type = httpParseErrorType::ExtraWhitespace,
                                        .what = "Request line contains extra whitespace between method and resource"}};

                            sp = requestLine.find(' ', lastProcessedIdx);

                            if(sp == std::string::npos)
                                return std::unexpected{httpParseError{ .type = httpParseErrorType::ExpectedMissingToken,
                                                                        .what = "Missing space delimiter between resource and version"}};

                            request.resource = requestLine.substr(lastProcessedIdx, sp - lastProcessedIdx);
                            
                            lastProcessedIdx = sp+1;

                            if(hasPrecedingWhitespace(requestLine.substr(lastProcessedIdx)))
                                return std::unexpected{httpParseError{ .type = httpParseErrorType::ExtraWhitespace,
                                        .what = "Request line contains extra whitespace between resource and version"}};

                            std::string_view version = requestLine.substr(lastProcessedIdx, lineEnd - lastProcessedIdx);

                            if(auto v = httpVersion::fromRequestLineVersion(version); !v.has_value() || v.value().major != ver.major)
                                return std::unexpected{httpParseError{std::string("Cannot parse version ") + std::string(version) + 
                                                        std::string(" with the specified parser version ") + ver.toString()}};

                            //TODO: assert if the server should return response as it's version even if the client is of another minor.

                            //must not return npos and we should have moved to the next line
                            lastProcessedIdx = lineEnd;
                            assert(lastProcessedIdx != std::string::npos);

                            //we do not sum +2 to the lastProcessedIdx bc the Headers must search for a double CRLF if there are no headers.
                            //In the case there are no headers, we must search for this CRLF and the one that marks the header end.
                            //if there are headers the +2 will be done in headers

                            std::clog << "[http.request_parser] state=RequestLine -> Headers\n";
                            
                            state = Processing::Headers;
                            break;
                        }
                        
                        //processes headers but does not search for the end of headers
                        case Processing::Headers:{
                            assert(request.method != httpMethod::INVALID);
                            size_t headerEnd;

                            std::clog << "[http.request_parser] state=Headers; searching header terminator\n";
                            //Headers are only processed if they get past this condition, which means all headers have arrived
                            //as this is the parser, we do not assume anything about the state of the connection.
                            // ww assume the parser receives a continuous stream of well formed data and that once we reach
                            //the end we return the request
                            if(headerEnd = constructed.find("\r\n\r\n", lastProcessedIdx); headerEnd == std::string::npos){
                                std::clog << "[http.request_parser] state=Headers; did not find CRLFCRLF for headers\n";
                                return httpParseStatus::NeedData;
                            }

                            //all that's next is only executed once due to the Headers having ended and therefore the state changing also

                            //there are no headers. As we delayed the +2 from the RequestLine we must do double here
                            //this is because we did yet not move the processIndex, which means that if we find headerEnd in the same position it is because a CRLF followed our
                            //our last CRLF from the Request line end
                            if(lastProcessedIdx == headerEnd){
                                lastProcessedIdx += 4; // to account for the double CRLF
                                state = Processing::Finished;
                                return httpParseStatus::Done;
                            }

                            //there is at least 1 header so we discard the CRLF from the request line
                            lastProcessedIdx += 2;

                            while(lastProcessedIdx < headerEnd){
                                size_t lineEnd;
                                
                                //TODO: NeedData returns are really calling for coroutines
                                if(lineEnd = constructed.find("\r\n", lastProcessedIdx); lineEnd == std::string::npos)
                                    return httpParseStatus::NeedData;

                                //header is between lastProcessedIdx and lineEnd
                                auto colon = constructed.find(':', lastProcessedIdx);

                                if(colon == std::string::npos)
                                    return std::unexpected{httpParseError{ .what = "Malformed http packet"}};

                                internal::Header header;

                                while(lastProcessedIdx != colon){
                                    header.key += constructed[lastProcessedIdx];
                                    lastProcessedIdx++;
                                }
                                //must have processed until lastProcessedIdx == colon
                                assert(lastProcessedIdx < lineEnd);

                                if(constructed[lastProcessedIdx+1] == ' ')
                                    lastProcessedIdx++;

                                while(lastProcessedIdx != lineEnd){
                                    header.value += constructed[lastProcessedIdx];
                                    lastProcessedIdx++;
                                }

                                //if this is the last header this will leave lastProcessedIdx-2 == headerEnd
                                lastProcessedIdx += 2;

                                request.headers.push_back(std::move(header));
                            }

                            //if the message contains a body, then it is mandatory that the content length header exists. This will
                            ///state us the end of the message. If there is none, then we have reached the end of the message

                            //for HEAD methods the conent length marks what could the length have been had it been a GET so we need to account
                            //for that. For now support for GET

                            //maybe we can use a map for faster access but generally there are very little headers and user
                            //can get a vector?
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
                                        httpParseError{ .what = std::string{"Content-Length field exceeds allowed range"} }
                                    };
                                }

                                if(bodySize < 0){
                                    return std::unexpected{
                                        httpParseError{ .what = std::string{"Expected Content-Length field to be non-negative, given "} + header.value}
                                    };
                                }

                                break;
                            }

                            //no body so the message is complete
                            if(bodySize == 0){
                                state = Processing::Finished;
                                return httpParseStatus::Done;
                            }

                            assert(lastProcessedIdx-2 == headerEnd);

                            lastProcessedIdx +=2;

                            std::clog << "[http.request_parser] state=Headers -> Body; body_size=" << bodySize << '\n';

                            state = Processing::Body;

                            break;
                        }
                        
                        case Processing::Body:{
                            //we start at the next character of the CRLF that separates the headers and body
                        
                            if(constructed.size() - lastProcessedIdx < bodySize)
                                return httpParseStatus::NeedData;

                            request.body.emplace();
                                
                            for(int i = 0; i != bodySize; i++)
                                request.body->push_back(constructed[lastProcessedIdx + i]);
                            
                            std::clog << "[http.request_parser] state=Body -> Finished\n";

                            state = Processing::Finished;

                            //if there is extra load we should ignore it
                            // if(constructed.size()-1 != lastProcessedIdx)
                            //     return append("");

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

            bool processingRequestLine() const noexcept{
                return state == Processing::RequestLine;
            }

            Request getRequest() noexcept{
                assert(state == Processing::Finished);
                reset();
                return std::move(request); //TODO: check the move constructor of Request and assert it leaves it as the default constructor (invalid)
            }

        private:

            void reset() noexcept{
                constructed.clear();
                lastProcessedIdx = std::string::npos;
                bodySize = -1;
                state = Processing::RequestLine;
            }

            static bool hasPrecedingWhitespace(std::string_view str){
                return str.starts_with(' ');
            }

            std::string constructed;
            std::string::size_type lastProcessedIdx = std::string::npos;
            //synced with the constructed string as new info gets received
            //TODO: assert this has a move constructor that leaves the object in it's original state, or at least a clear method
            Request request;
            std::ptrdiff_t bodySize = -1; // maybe use something bigger in the future?
            Processing state = Processing::RequestLine;
    };
} // namespace ninttp::internal
