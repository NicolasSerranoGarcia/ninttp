#pragma once

#include <string>
#include <cstddef>
#include <sstream>
#include <optional>
#include <cassert>

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
            std::optional<httpParseError> append(const std::string& received){
                constructed.append(received);
                return std::nullopt;
            }

            bool finished() noexcept{
                return state == Processing::Finished;
            }

            Response getResponse() noexcept{
                assert(state == Processing::Finished);
                return std::move(request); //TODO: check the move constructor of Request and assert it leaves it as the default constructor (invalid)
            }

        private:

            std::string constructed;
            std::ptrdiff_t lastProcessedIdx = -1;
            //synced with the constructed string as new info gets received
            Response request;
            std::ptrdiff_t bodySize = -1; // maybe use something bigger in the future?
            Processing state = Processing::StatusLine;
    };
} // namespace ninttp::internal
