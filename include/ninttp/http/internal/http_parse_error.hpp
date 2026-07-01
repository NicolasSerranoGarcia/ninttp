#pragma once

#include <string>

namespace ninttp::internal
{
    enum class httpParseStatus{
        NeedData,
        Done
    };

    enum class httpParseErrorType{
        UnrecognizedToken,
        ExpectedMissingToken,
        ExtraWhitespace,
        UnsupportedVersion,
        UnrecognizedVersion,
        RequestLineTooLong,
        TargetTooLong,
        MethodTooLong,
        VersionTooLong
    };

    struct httpParseError{
        std::string what;
        std::string parseContextText;
        httpParseErrorType type;
    };
} // namespace ninttp::internal
