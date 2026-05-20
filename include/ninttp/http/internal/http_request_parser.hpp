#pragma once

#include <string>
#include <cstddef>
#include <sstream>
#include <optional>
#include <cassert>

#include "../types.hpp"


namespace ninttp::internal
{

    struct httpParseError{
        std::string what;
    };

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
            std::optional<httpParseError> append(const std::string& received){
                constructed.append(received);
                std::istringstream ss(received);
                switch(state){
                    //first time parsing. Store until we find CRLF and then process once
                    case Processing::RequestLine:{
                        size_t lineEnd;
                        if(lineEnd = constructed.find("\r\n"); lineEnd != std::string::npos)
                            return std::nullopt;

                        std::string verb;
                        std::string version;
                        //this is incorrect bc we are not assured the whole line has been sent. We should store in a buffer until we see the CRLF
                        ss >> verb >> request.resource >> version;

                        //for now return an error but we could simply ignore the message bc it is not malformed if it is a different supported version
                        if(auto v = httpVersion::fromRequestLine(version); !v.has_value() || v.value().major != ver.major)
                            return std::make_optional(httpParseError{"Cannot parse version " + version + 
                                                    " with the specified version " + ver.toString()});

                        for(const auto v : all_verbs){
                            if(httpVerbStr[static_cast<const int>(v)] == verb)
                                request.op = v;
                        }

                        if(request.op == httpMethod::INVALID)
                            return std::make_optional(httpParseError{"Unrecognized method: " + verb});

                        //must not return npos and we should have moved to the next line
                        lastProcessedIdx = lineEnd;
                        assert(lastProcessedIdx != std::string::npos);

                        //we do not sum +2 to the lastProcessedIdx bc the Headers must search for a double CRLF if there are no headers
                        //In the case there are no headers, we must search for this CRLF and the one that marks the header end.
                        //if there are headers the +2 will be done in headers

                        state = Processing::Headers;
                        return std::nullopt;
                    }

                    //processes headers but does not search for the end of headers
                    case Processing::Headers:{
                        assert(request.op != httpMethod::INVALID);
                        size_t headerEnd;

                        //Headers are only processed if they get past this condition, which means all headers have arrived
                        //as this is the parser, we do not assume anything about the state of the connection.
                        // ww assume the parser receives a continuous stream of well formed data and that once we reach
                        //the end we return the request
                        if(headerEnd = constructed.find_first_of("\r\n\r\n", lastProcessedIdx); headerEnd == std::string::npos)
                            return std::nullopt;

                        //all that's next is only executed once due to the Headers having ended and therefore the state changing also

                        //there are no headers. As we delayed the +2 from the RequestLine we must do double here
                        if(lastProcessedIdx == headerEnd){
                            lastProcessedIdx += 4; // to account for the double CRLF
                            state = Processing::Body; // TODO: check the whole state machine bc this might not be correct at all
                            return std::nullopt;
                        }

                        //there is at least 1 so we discard the CRLF from the request line
                        lastProcessedIdx += 2;

                        while(lastProcessedIdx < headerEnd){
                            size_t lineEnd;
                            
                            if(lineEnd = constructed.find_first_of("\r\n", lastProcessedIdx); lineEnd == std::string::npos)
                                return std::nullopt;

                            //header is between lastProcessedIdx and lineEnd
                            auto colon = constructed.find(':', lastProcessedIdx);

                            assert(colon != std::string::npos);

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
                        for(const auto& header : request.headers)
                            if(header.key == std::string("Content-Length"))
                                bodySize = std::stoi(header.value);

                        //no body so the message is complete
                        if(bodySize == -1 || bodySize == 0){
                            state = Processing::Finished;
                            return std::nullopt;
                        }

                        assert(lastProcessedIdx-2 == headerEnd);

                        lastProcessedIdx +=2;

                        state = Processing::Body;
                        return std::nullopt;
                    }

                    case Processing::Body:{
                        //we start at the next character of the CRLF that separates the headers and body
                    
                        if(constructed.size() - lastProcessedIdx < bodySize)
                            return std::nullopt;

                        for(int i = 0; i != bodySize; i++)
                            request.body += constructed[lastProcessedIdx + i];
                        
                        state = Processing::Finished;
                        return std::nullopt;
                    }
                }

                return std::nullopt;
            }

            bool finished() noexcept{
                return state == Processing::Finished;
            }

            Request getRequest() noexcept{
                assert(state == Processing::Finished);
                return std::move(request); //TODO: check the move constructor of Request and assert it leaves it as the default constructor (invalid)
            }

        private:

            std::string constructed;
            std::ptrdiff_t lastProcessedIdx = -1;
            //synced with the constructed string as new info gets received
            Request request;
            std::ptrdiff_t bodySize = -1; // maybe use something bigger in the future?
            Processing state = Processing::RequestLine;
    };
} // namespace ninttp::internal
