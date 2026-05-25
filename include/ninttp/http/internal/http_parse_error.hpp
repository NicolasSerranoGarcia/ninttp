#pragma once

#include <string>

namespace ninttp::internal
{
    struct httpParseError{
        std::string what;
    };
} // namespace ninttp::internal