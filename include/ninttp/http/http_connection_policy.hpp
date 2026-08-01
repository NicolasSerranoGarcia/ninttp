#pragma once

#include <chrono>
#include <cstddef>

namespace ninttp
{
    enum class httpPipeliningPolicy{
        Disabled,
        Sequential
    };

    struct httpConnectionPolicy{
        bool allowHttp10KeepAlive = true;
        httpPipeliningPolicy pipelining = httpPipeliningPolicy::Sequential;
        std::size_t maxRequests = 100;
        std::size_t maxPipelinedBytes = 64 * 1024;
        std::chrono::milliseconds idleTimeout = std::chrono::seconds{60};
        std::chrono::milliseconds incompleteRequestTimeout = std::chrono::seconds{15};
        std::chrono::milliseconds responseTimeout = std::chrono::seconds{30};
        std::chrono::milliseconds writeTimeout = std::chrono::seconds{30};
        std::chrono::milliseconds gracefulCloseTimeout = std::chrono::seconds{5};
    };
} // namespace ninttp
