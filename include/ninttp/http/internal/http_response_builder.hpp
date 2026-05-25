#include <string>
#include "../types.hpp"

namespace ninttp::internal{

    //single responsibility. The response builder assumes the Response object is to be built as-is. Some basic validation maybe
    //but this classes only responsibility is to build the response string to be sent exactly, that's it.
    class httpResponseBuilder{
        public: 
            static std::string fromResponseObject(const Response& response){
                std::string responseStr;

                responseStr += response.version.toHeaderString();

                responseStr += " ";

                //maybe change the status code to have a toString method for changes in future API
                responseStr += std::to_string(response.statusCode);

                responseStr += " ";

                responseStr += getReadableStatus(response.statusCode);

                responseStr += "\r\n";

                for(const auto& header : response.headers){
                    responseStr += header.key + std::string(": ") + header.value;
                }

                responseStr += "\r\n";

                if(response.body.has_value()){
                    //TODO
                    //assert(response.headers.contains("Content-Length"))
                    responseStr += response.body.value();
                }

                //no move for NRVO
                return responseStr;
            }
    };
} // namesapce ninttp::internal