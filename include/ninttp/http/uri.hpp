#pragma once

#include <charconv>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <boost/url.hpp>

namespace ninttp
{
    enum class UriErrorType{
        InvalidAuthority,
        UserInfoNotAllowed,
        EmptyHost,
        InvalidPort,
        InvalidRequestTarget
    };

    struct UriError{
        UriErrorType type;
        std::string what;
    };

    struct QueryParameter{
        std::string key;
        std::optional<std::string> value;

        bool operator==(const QueryParameter&) const = default;
    };

    class Authority{
        public:
            [[nodiscard]] static std::expected<Authority, UriError>
            parseHost(std::string_view value){
                const auto parsed = boost::urls::parse_authority(
                    boost::core::string_view{value.data(), value.size()});
                if(!parsed)
                    return std::unexpected{UriError{
                        .type = UriErrorType::InvalidAuthority,
                        .what = "Invalid HTTP authority: " + parsed.error().message()
                    }};

                const auto authority = parsed.value();
                if(authority.has_userinfo())
                    return std::unexpected{UriError{
                        .type = UriErrorType::UserInfoNotAllowed,
                        .what = "HTTP Host authority cannot contain user information"
                    }};

                const auto encodedHost = authority.encoded_host();
                if(encodedHost.empty())
                    return std::unexpected{UriError{
                        .type = UriErrorType::EmptyHost,
                        .what = "HTTP Host authority cannot contain an empty host"
                    }};

                Authority result;
                result.encoded_.assign(value);
                result.host_.assign(encodedHost.data(), encodedHost.size());
                result.normalizedHost_ = result.host_;
                for(char& character : result.normalizedHost_){
                    if(character >= 'A' && character <= 'Z')
                        character = static_cast<char>(character + ('a' - 'A'));
                }

                if(authority.has_port()){
                    const auto rawPort = authority.port();
                    if(rawPort.empty())
                        return std::unexpected{UriError{
                            .type = UriErrorType::InvalidPort,
                            .what = "HTTP Host authority contains an empty port"
                        }};

                    std::uint16_t port = 0;
                    const char* first = rawPort.data();
                    const char* last = first + rawPort.size();
                    const auto converted = std::from_chars(first, last, port);
                    if(converted.ec != std::errc{} || converted.ptr != last)
                        return std::unexpected{UriError{
                            .type = UriErrorType::InvalidPort,
                            .what = "HTTP Host authority port is outside the valid range"
                        }};

                    result.port_ = port;
                }

                return result;
            }

            [[nodiscard]] const std::string& encoded() const noexcept{
                return encoded_;
            }

            [[nodiscard]] const std::string& host() const noexcept{
                return host_;
            }

            [[nodiscard]] const std::string& normalizedHost() const noexcept{
                return normalizedHost_;
            }

            [[nodiscard]] const std::optional<std::uint16_t>& port() const noexcept{
                return port_;
            }

            [[nodiscard]] std::uint16_t effectivePort(
                std::uint16_t defaultPort = 80) const noexcept
            {
                return port_.value_or(defaultPort);
            }

            [[nodiscard]] std::string routingKey(
                std::uint16_t defaultPort = 80) const
            {
                return normalizedHost_ + ':' + std::to_string(effectivePort(defaultPort));
            }

        private:
            std::string encoded_;
            std::string host_;
            std::string normalizedHost_;
            std::optional<std::uint16_t> port_;
    };

    class RequestTarget{
        public:
            RequestTarget() = default;

            [[nodiscard]] static std::expected<RequestTarget, UriError>
            parseOriginForm(std::string_view value){
                const auto parsed = boost::urls::parse_origin_form(
                    boost::core::string_view{value.data(), value.size()});
                if(!parsed)
                    return std::unexpected{UriError{
                        .type = UriErrorType::InvalidRequestTarget,
                        .what = "Invalid HTTP origin-form request target: " + parsed.error().message()
                    }};

                const auto target = parsed.value();
                RequestTarget result;
                result.encoded_.assign(value);

                const auto encodedPath = target.encoded_path();
                result.path_.assign(encodedPath.data(), encodedPath.size());

                if(target.has_query()){
                    const auto encodedQuery = target.encoded_query();
                    result.query_.emplace(encodedQuery.data(), encodedQuery.size());

                    for(const auto parameter : target.params()){
                        QueryParameter stored{
                            .key = std::string(parameter.key.data(), parameter.key.size())
                        };
                        if(parameter.has_value)
                            stored.value.emplace(parameter.value.data(), parameter.value.size());
                        result.parameters_.push_back(std::move(stored));
                    }
                }

                return result;
            }

            [[nodiscard]] const std::string& encoded() const noexcept{
                return encoded_;
            }

            [[nodiscard]] const std::string& path() const noexcept{
                return path_;
            }

            [[nodiscard]] const std::optional<std::string>& query() const noexcept{
                return query_;
            }

            [[nodiscard]] const std::vector<QueryParameter>& parameters() const noexcept{
                return parameters_;
            }

        private:
            std::string encoded_;
            std::string path_;
            std::optional<std::string> query_;
            std::vector<QueryParameter> parameters_;
    };
} // namespace ninttp
