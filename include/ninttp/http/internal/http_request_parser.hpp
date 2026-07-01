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

        enum class RequestLineProcessing{
            Method,
            Target,
            Version
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
                        case Processing::RequestLine:{
                            if(constructed.size() > MaxRequestLineLength)
                                return std::unexpected{httpParseError{ .type = httpParseErrorType::RequestLineTooLong,
                                                                        .what = "Request line exceeds total length allowed by the RFC 9112"}};

                            switch(requestLineState){
                                case RequestLineProcessing::Method:{
                                    assert(lastProcessedIdx == std::string::npos);

                                    if(hasPrecedingWhitespace(constructed))
                                        return std::unexpected{httpParseError{ .type = httpParseErrorType::ExtraWhitespace,
                                                                                .what = "Request line contains preceeding whitespace"}};

                                    //SYNC POINT: space delimited between the method and the resource
                                    auto methodResourceSP = constructed.find(' ');

                                    if(methodResourceSP == std::string::npos){
                                        if(constructed.size() > MaxMethodLength)
                                            return std::unexpected{httpParseError{ .type = httpParseErrorType::MethodTooLong, 
                                                                                .what = "Method length exceeds max length possible for a standard method"}};

                                        return httpParseStatus::NeedData;
                                    }

                                    std::string_view lineMethod = std::string_view{constructed}.substr(0, methodResourceSP);

                                    for(const auto m : allHttpMethods){
                                        if(allHttpMetodsStr[static_cast<const int>(m)] == lineMethod)
                                            request.method = m;
                                    }

                                    if(request.method == httpMethod::INVALID)
                                        return std::unexpected{httpParseError{ .type = httpParseErrorType::UnrecognizedToken,
                                                                                .what = std::string("Unrecognized method: ") + std::string(lineMethod)}};

                                    lastProcessedIdx = methodResourceSP+1;
                                    requestLineState = RequestLineProcessing::Target;
                                    break;
                                }

                                case RequestLineProcessing::Target:{

                                    if(hasPrecedingWhitespace(std::string_view{constructed}.substr(lastProcessedIdx)))
                                        return std::unexpected{httpParseError{ .type = httpParseErrorType::ExtraWhitespace,
                                                                                .what = "Request line contains extra whitespace between method and target"}};

                                    if(std::string_view{constructed}.substr(lastProcessedIdx).size() > MaxRequestTargetLength)
                                        return std::unexpected{httpParseError{ .type = httpParseErrorType::TargetTooLong, 
                                                                            .what = "Target length exceeds max length possible for RFC 9112"}};

                                    //SYNC POINT: space delimited between the resource and the version
                                    auto targetVersionSP = constructed.find(' ', lastProcessedIdx);
                                    auto requestLineEnd = constructed.find("\r\n", lastProcessedIdx);

                                    //avoid reporting the need for more data when we find a CRLF but not a SP, as CRLF is reserved only for the request line delimiter
                                    if(requestLineEnd != std::string::npos && (targetVersionSP == std::string::npos || requestLineEnd < targetVersionSP))
                                        return std::unexpected{httpParseError{ .type = httpParseErrorType::ExpectedMissingToken,
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
                                                                                .what = "Request line contains extra whitespace between target and version"}};

                                    //SYNC POINT: CRLF delimiting request line
                                    std::string::size_type requestLineEnd = constructed.find("\r\n", lastProcessedIdx);

                                    std::string::size_type versionLength = requestLineEnd == std::string::npos ? requestLineEnd : requestLineEnd - lastProcessedIdx;

                                    //search until the end of constructed in case we havent found CRLF, if not until CRLF to avoid including parts of header
                                    if(std::string_view{constructed}.substr(lastProcessedIdx, versionLength).size() > HTTPVersionLength)
                                        return std::unexpected{httpParseError{ .type = httpParseErrorType::VersionTooLong,
                                                                                .what = "Version length exceeds expected length of 8 bytes (HTTP/X.X)"}};

                                    if(requestLineEnd == std::string::npos)
                                        return httpParseStatus::NeedData;

                                    std::string_view version = std::string_view{constructed}.substr(lastProcessedIdx, requestLineEnd - lastProcessedIdx);

                                    if(hasTrailingWhitespace(version))
                                        return std::unexpected{httpParseError{ .type = httpParseErrorType::ExtraWhitespace,
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
                                                                                .what = std::string("Unrecognized version from request line ") + std::string(version)}};

                                    if(v.value().major > ver.major)
                                        return std::unexpected{httpParseError{ .type = httpParseErrorType::UnsupportedVersion, 
                                                                                .what = std::string("Cannot parse request with version ") + std::string(version) + 
                                                                                std::string(" using the specified parser version ") + ver.toString()}};

                                    request.version = v.value();

                                    lastProcessedIdx = requestLineEnd;
                                    assert(lastProcessedIdx != std::string::npos);

                                    //we do not sum +2 to the lastProcessedIdx bc the Headers must search for a double CRLF if there are no headers.
                                    //In the case there are no headers, we must search for this CRLF and the one that marks the header end.
                                    //if there are headers the +2 will be done in headers

                                    std::clog << "[http.request_parser] state=RequestLine -> Headers\n";
                                    
                                    state = Processing::Headers;
                                    break;
                                } // case RequestLineProcessing::Version
                            } // switch(state)
                            break;
                        } // case Processing::RequestLine

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
                requestLineState = RequestLineProcessing::Method;
            }

            static bool hasPrecedingWhitespace(std::string_view str){
                return str.starts_with(' ');
            }

            static bool hasPrecedingWhitespace(const std::string& str){
                return str.starts_with(' ');
            }

            static bool hasTrailingWhitespace(std::string_view str){
                return str.ends_with(' ');
            }

            std::string constructed;
            std::string::size_type lastProcessedIdx = std::string::npos;
            //synced with the constructed string as new info gets received
            //TODO: assert this has a move constructor that leaves the object in it's original state, or at least a clear method
            Request request;
            std::ptrdiff_t bodySize = -1; // maybe use something bigger in the future?
            Processing state = Processing::RequestLine;
            RequestLineProcessing requestLineState = RequestLineProcessing::Method;

            static constexpr const std::size_t MaxMethodLength = 32;
            static constexpr const std::size_t MaxRequestTargetLength = 8000;
            static constexpr const std::size_t HTTPVersionLength = 8;
            static constexpr const std::size_t MaxRequestLineLength = MaxMethodLength + MaxRequestTargetLength + 4 /*2 SP & CRLF*/ + HTTPVersionLength;
    };
} // namespace ninttp::internal
