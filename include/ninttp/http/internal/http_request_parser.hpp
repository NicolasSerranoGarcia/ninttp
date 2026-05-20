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

    //builder pattern? receive data that builds a request,
    //and when reaching critical points (end of message, headers finish) maybe externalize state.
    //in any of the cases once it is ready release the response object fully built
    template<httpVersion ver = http_1_0>
    class httpRequestParser{

        enum class CurState{
            Default,
            Finished,
            Headers,
            FLine,
            Body
        };

        public:
            //most certainly synced with a stream socket connection
            std::optional<httpParseError> append(const std::string& received){
                constructed.append(received);
                std::istringstream ss(received);
                switch(state){
                    //first time parsing. Store until we find CRLF and then process once
                    case CurState::Default:{
                        size_t lineEnd;
                        if(lineEnd = constructed.find("\r\n"); lineEnd != std::string::npos)
                            return std::nullopt;

                        std::string verb;
                        std::string version;
                        //this is incorrect bc we are not assured the whole line has been sent. We should store in a buffer until we see the CRLF
                        ss >> verb >> request.resource >> version;

                        //for now return an error but we could simply ignore the message bc it is not malformed if it is a different supported version
                        if(auto v = httpVersion::fromHeader(version); !v.has_value() || v.value().major != ver.major)
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

                        state = CurState::Headers;
                        return std::nullopt;
                    }

                    //processes headers but does not search for the end of headers
                    case CurState::Headers:{
                        size_t headerEnd;
                        //this is not correct bc finding it does not mean we have processed all the remaining headers
                        if(headerEnd = constructed.find_first_of("\r\n\r\n", lastProcessedIdx); headerEnd == std::string::npos)
                            return std::nullopt;

                        while(lastProcessedIdx != headerEnd){
                            size_t lineEnd;
                            //we process all headers so only the last one 
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

                            //here we need to sum 2 to lastProccessedIdx to account for the \r\n

                            request.headers.push_back(std::move(header));
                            
                        }
                    }

                }
            }

        private:

            std::string constructed;
            std::ptrdiff_t lastProcessedIdx = -1;
            //synced with the constructed string as new info gets received
            Request request;
            CurState state = CurState::Default;
    };
} // namespace ninttp::internal
