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
        UnsupportedVersion, //505 HTTP Version Not supported
        UnrecognizedVersion, //400 Bad Request
        RequestLineTooLong,
        TargetTooLong,
        MethodTooLong,
        VersionTooLong,
        InvalidLength,
        BodyTooLarge,
        HeaderFieldsTooLarge,
        DuplicatedHeader,
        IncompatibleHeaders, //400 Bad Request,
        MissingHostHeader,
        InvalidAuthority,
        InvalidRequestTarget,
        DisallowedTokenChar,
        InvalidHeaderFormat, //400 probably
        UnimplementedFeature, //501
        UnexpectedToken
    };

    struct httpParseError{
        httpParseErrorType type;
        std::string parseContextText;
        std::string what;

        [[nodiscard]] std::string message() const{
            if(parseContextText.empty())
                return what;

            return what + std::string{"\nParse context: "} + parseContextText;
        }
    };
} // namespace ninttp::internal
