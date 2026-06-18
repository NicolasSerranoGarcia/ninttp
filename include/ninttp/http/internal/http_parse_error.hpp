#pragma once

#include <string>

namespace ninttp::internal
{
    enum class httpParseStatus{
        NeedData,
        Done
    };

    struct httpParseError{
        std::string what;
    };
} // namespace ninttp::internal
