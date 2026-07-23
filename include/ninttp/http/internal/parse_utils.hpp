#pragma once

#include <string>
#include <string_view>

namespace ninttp::utils{
    constexpr static bool hasPrecedingWhitespace(std::string_view str){
        return str.starts_with(' ');
    }

    constexpr static bool hasPrecedingWhitespace(const std::string& str){
        return str.starts_with(' ');
    }

    constexpr static bool hasTrailingWhitespace(std::string_view str){
        return str.ends_with(' ');
    }

    constexpr static char asciiLower(char c) noexcept{
        if(c >= 'A' && c <= 'Z')
            return static_cast<char>(c + ('a' - 'A'));

        return c;
    }

    constexpr static std::string toLower(std::string str) noexcept{
        for(char& c : str)
            c = asciiLower(c);

        return str;
    }

    static constexpr bool isTChar(char c) noexcept {
        return (c >= 'A' && c <= 'Z') ||
            (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') ||
            c == '!' || c == '#' || c == '$' || c == '%' ||
            c == '&' || c == '\'' || c == '*' || c == '+' ||
            c == '-' || c == '.' || c == '^' || c == '_' ||
            c == '`' || c == '|' || c == '~';
    }

    static constexpr bool isChunkExtensionWhitespace(char c) noexcept{
        return c == ' ' || c == '\t';
    }

    static constexpr bool isQuotedText(char c) noexcept{
        const auto byte = static_cast<unsigned char>(c);
        return c == '\t' || c == ' ' || c == '!' ||
            (byte >= 0x23 && byte <= 0x5b) ||
            (byte >= 0x5d && byte <= 0x7e) ||
            byte >= 0x80;
    }

    static constexpr bool isQuotedPairValue(char c) noexcept{
        const auto byte = static_cast<unsigned char>(c);
        return c == '\t' || c == ' ' ||
            (byte >= 0x21 && byte <= 0x7e) ||
            byte >= 0x80;
    }
} // namespace ninttp::utils
