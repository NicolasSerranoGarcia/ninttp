#pragma once

#include <string>
#include <cstddef>
#include <sstream>

#include "../types.hpp"


namespace ninttp::internal
{

    //builder pattern? receive data that builds a request,
    //and when reaching critical points (end of message, headers finish) maybe externalize state.
    //in any of the cases once it is ready release the response object fully built
    template<httpVersion ver = httpVersion::_1_0>
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
            bool append(const std::string& received){
                constructed.append(received);
                std::ostringstream ss(received);
                switch(state){
                    //first time parsing. lastProcessedIdx must be -1
                    case CurState::Default:{
                        std::string verb;
                        std::string version;
                        ss >> verb >> request.resource >>version;

                        
                    }
                }
            }

        private:
            void processAll(){
                
            }

            std::string constructed;
            std::ptrdiff_t lastProcessedIdx = -1;
            //synced with the constructed string as new info gets received
            Request request;
            CurState state = CurState::Default;
    };
} // namespace ninttp::internal
