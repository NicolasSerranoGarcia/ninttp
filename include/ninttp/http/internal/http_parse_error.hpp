#pragma once

#include <string>

namespace ninttp::internal
{
    enum class httpParseStatus{
        NeedData,
        Done
    };

    //TODO: map to its http errors
    enum class httpParseErrorType{
        UnrecognizedToken,
        ExpectedMissingToken,
        ExtraWhitespace,
        UnsupportedVersion, //505 HTTP Version Not supported
        UnrecognizedVersion, //400 Bad Request
        RequestLineTooLong,
        TargetTooLong,
        MethodTooLong,
        VersionTooLong,
        InvalidLength,
        DuplicatedHeader,
        IncompatibleHeaders //400 Bad Request
    };

    struct httpParseError{
        std::string what;
        std::string parseContextText;
        httpParseErrorType type;
    };
} // namespace ninttp::internal
