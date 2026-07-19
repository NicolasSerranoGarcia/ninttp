#pragma once

#include <array>
#include <string_view>

#if defined(__has_include)
    #if __has_include(<ninttp/ninttp_http_method_config.hpp>)
        #include <ninttp/ninttp_http_method_config.hpp>
    #endif
#endif

#ifndef NINTTP_HTTP_METHOD_CONFIG_GENERATED
namespace ninttp::internal{
    inline constexpr std::array<std::string_view, 0> configuredHttpExtensionMethods{};
}
#endif

namespace ninttp::internal{
    // RFC 9110 Section 9.1 defines method tokens as case-sensitive. Keep these
    // comparisons exact; differently cased tokens are distinct extension methods.
    [[nodiscard]] constexpr bool isStandardHttpMethod(std::string_view method) noexcept{
        return method == "GET" ||
               method == "HEAD" ||
               method == "POST" ||
               method == "PUT" ||
               method == "DELETE" ||
               method == "CONNECT" ||
               method == "OPTIONS" ||
               method == "TRACE" ||
               method == "PATCH";
    }

    [[nodiscard]] constexpr bool isConfiguredHttpExtensionMethod(std::string_view method) noexcept{
        for(const auto configuredMethod : configuredHttpExtensionMethods){
            if(configuredMethod == method)
                return true;
        }

        return false;
    }

    [[nodiscard]] constexpr bool isSupportedHttpMethod(std::string_view method) noexcept{
        return isStandardHttpMethod(method) || isConfiguredHttpExtensionMethod(method);
    }
}
