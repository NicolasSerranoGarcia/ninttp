#pragma once

#include <cstdlib>
#include <iostream>

namespace ninttp::tests{
    inline void check(bool condition, const char* expression, const char* file, int line){
        if(condition)
            return;

        std::cerr << file << ':' << line << ": check failed: " << expression << '\n';
        std::abort();
    }
}

#define NINTTP_CHECK(expression) \
    ::ninttp::tests::check(static_cast<bool>(expression), #expression, __FILE__, __LINE__)
