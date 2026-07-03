#include <string>

#include "../types.hpp"

namespace ninttp::internal{

    //single responsibility. The response builder assumes the Response object is to be built as-is. Some basic validation maybe
    //but this classes only responsibility is to build the response string to be sent exactly, that's it.
    //template the version to match the server one
    class httpResponseBuilder{
    };
} // namesapce ninttp::internal
