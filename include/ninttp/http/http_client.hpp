#pragma once

#include <array>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <expected>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include "../endpoints.hpp"
#include "../error/nin_error.hpp"
#include "../socket/socket.hpp"
#include "../socket/traits.hpp"
#include "internal/http_response_parser.hpp"
#include "internal/http_request_builder.hpp"
#include "http_limits.hpp"
#include "types.hpp"

namespace ninttp
{
    template<httpVersion ver = http_1_0, typename EndpointT = IPv4Endpoint>
    class httpClient{
        static_assert(isSupportedHTTP1Version(ver),
            "HTTP client only supports HTTP/1.0 and HTTP/1.1");
        static_assert(std::same_as<EndpointT, IPv4Endpoint> || std::same_as<EndpointT, IPv6Endpoint>,
            "HTTP client only accepts IPv4 or IPv6 endpoints");

        public:

            httpClient() = delete;

            /**
             * @brief Construct a client and connect it to @p peer.
             *
             * @throws NinError with .type = Socket if stream socket construction or connection fails.
             */
            httpClient(const EndpointT& peer, std::string_view host) try
                : defaultHost(validatedHostOrThrow(host)),
                  streamSock_(Protocol::Tcp)
            {
                if(const auto res = streamSock_.connect(peer); !res.has_value())
                    throw NinError::fromSocketError(res.error());
            }
            catch(const SocketError& error){
                throw NinError::fromSocketError(error);
            }

            std::expected<void, NinError> setDefaultHost(std::string_view host){
                auto validated = validateHost(host);
                if(!validated.has_value())
                    return std::unexpected{std::move(validated).error()};

                defaultHost = std::move(validated).value();
                return {};
            }

            std::expected<Response, NinError> GET(const std::string& target){
                
                internal::httpRequestBuilder<ver> builder{"GET"};

                if(auto setTarget = builder.setTarget(target); !setTarget.has_value())
                    return std::unexpected{std::move(setTarget).error()};
                if(auto setHost = builder.setHost(defaultHost); !setHost.has_value())
                    return std::unexpected{std::move(setHost).error()};

                auto requestStr = builder.get().toString();

                if(auto sent = streamSock_.sendAll(requestStr); !sent.has_value())
                    return std::unexpected{NinError::fromSocketError(sent.error())};

                return parseResponse(streamSock_);
            }

        private:
            [[nodiscard]] static std::expected<std::string, NinError>
            validateHost(std::string_view host){
                if(host.size() > limits::MaxFieldValueLength)
                    return invalidAuthority(host, "Host authority exceeds the configured field value limit");

                auto authority = Authority::parseHost(host);
                if(!authority.has_value())
                    return invalidAuthority(host, authority.error().what);

                return authority->encoded();
            }

            [[nodiscard]] static std::string validatedHostOrThrow(std::string_view host){
                auto validated = validateHost(host);
                if(!validated.has_value())
                    throw std::move(validated).error();

                return std::move(validated).value();
            }

            [[nodiscard]] static std::unexpected<NinError>
            invalidAuthority(std::string_view host, std::string message){
                return std::unexpected{NinError::fromHttpParseError(internal::httpParseError{
                    .type = internal::httpParseErrorType::InvalidAuthority,
                    .parseContextText = std::string{host},
                    .what = std::move(message)
                })};
            }

            std::string defaultHost;
            StreamSocket<EndpointT> streamSock_;

            std::expected<Response, NinError> parseResponse(StreamSocket<EndpointT>& sock){
                internal::httpResponseParser<ver> parser{"GET"};
                std::string got;

                std::array<char, limits::ReadBufferSize> buf{};

                auto htppParseStatus = internal::httpParseStatus::NeedData;
                do{
                    auto res = sock.receive(buf);

                    if(!res.has_value()){
                        const SocketError& err = res.error();
                        if(err.category() == SocketErrorCategory::Interrupted)
                            continue;

                        if(err.category() == SocketErrorCategory::ConnectionClosed){
                            auto finished = parser.finish();
                            if(!finished.has_value())
                                return std::unexpected{NinError::fromHttpParseError(finished.error())};

                            return parser.getResponse();
                        }

                        return std::unexpected{NinError::fromSocketError(err)};
                    }

                    std::size_t read = res.value();

                    if(read == 0){
                        auto finished = parser.finish();
                        if(!finished.has_value())
                            return std::unexpected{NinError::fromHttpParseError(finished.error())};

                        return parser.getResponse();
                    }

                    got.append(buf.data(), read);

                    auto parseRes = parser.append(got);
                    got.clear();

                    if(!parseRes.has_value())
                        return std::unexpected{NinError::fromHttpParseError(parseRes.error())};

                    htppParseStatus = *parseRes;
                    if(htppParseStatus == internal::httpParseStatus::Done){
                        assert(parser.finished());
                        break;
                    }
                }while(htppParseStatus == internal::httpParseStatus::NeedData);

                return parser.getResponse();
            }
    };
} // namespace ninttp
