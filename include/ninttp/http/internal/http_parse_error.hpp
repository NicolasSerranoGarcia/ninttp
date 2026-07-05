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
        IncompatibleHeaders, //400 Bad Request,
        MissingHostHeader,
        DisallowedTokenChar,
        InvalidHeaderFormat
    };

    struct httpParseError{
        httpParseErrorType type;
        std::string what;
        std::string parseContextText;
    };
} // namespace ninttp::internal
