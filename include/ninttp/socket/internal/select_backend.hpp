
#pragma once

//to avoid namespace conflicts we make the includes outside of the namespace. The "using" is still done inside the namespace
//it turns out that if we put the backend selection inside the namespace (the #include) we would end up with a namespace
//like ninttp::internal::ninttp::internal (include directives just replace the call with the actual code, so makes sense)
//the replacement would look like:
//ninttp::internal{
//    #if defined(_WIN32)
//    namespace ninttp::internal{
//          class WinsockBackend{ ... }
//    }
//}
#include "../../nintraits.hpp"

#if NINTTP_BACKEND_WINSOCK == 1
    #include "backends/winsock.hpp"
#elif NINTTP_BACKEND_POSIX == 1
    #include "backends/posix.hpp"
#else
    #error "ninttp: No supported socket backend selected."
#endif

namespace ninttp::internal
{
    #if NINTTP_BACKEND_WINSOCK == 1
        using SelectedBackend = WinsockBackend;
    #elif NINTTP_BACKEND_POSIX == 1
        using SelectedBackend = PosixBackend;
    #endif
} // namespace ninttp::internal
