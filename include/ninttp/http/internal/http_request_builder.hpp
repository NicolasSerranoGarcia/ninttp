#include <string>

#include "../types.hpp"

namespace ninttp{
    template <internal::httpMethod mode>
    class httpRequestBuilder{
        //use body at own risk. Depending on the implementation, a GET with a body might get rejected or have ambiguous behavior.
        //it doesnt offer semantic meaning either, but it is left as a lifeline.
        static Request build(std::string target, std::string body = ""){
            Request request;

            request.method = mode;
        }
    };
} //namesapce ninttp
