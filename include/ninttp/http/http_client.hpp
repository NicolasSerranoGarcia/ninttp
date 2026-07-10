#pragma once

#include <array>
#include <cassert>
#include <concepts>
#include <expected>
#include <iostream>
#include <stdexcept>
#include <utility>
#include <vector>

#include "../endpoints.hpp"
#include "../error/nin_error.hpp"
#include "../socket/socket.hpp"
#include "../socket/traits.hpp"
#include "internal/http_response_parser.hpp"
#include "internal/http_request_builder.hpp"
#include "types.hpp"

namespace ninttp
{
    template<httpVersion ver = http_1_0, typename EndpointT = IPv4Endpoint>
    class httpClient{
        static_assert(std::same_as<EndpointT, IPv4Endpoint> || std::same_as<EndpointT, IPv6Endpoint>,
            "HTTP client only accepts IPv4 or IPv6 endpoints");

        public:

            httpClient() = delete;

            /**
             * @brief Construct a client and connect it to @p peer.
             *
             * @throws NinError with .type = Socket if stream socket construction or connection fails.
             */
            httpClient(const EndpointT& peer, const std::string& host)
                : streamSock_(Protocol::Tcp)
            {
                if(const auto res = streamSock_.connect(peer); !res.has_value())
                    throw NinError::fromSocketError(res.error());

                if(host.empty())
                    throw std::out_of_range("expected host value parameter to contain a non-empty string");

                if(host.size() > MAXFIELDVALUESIZE)
                    throw std::out_of_range("Host value exceeds maximum header field length of 256");

                defaultHost = host;
            }

            std::expected<void, std::out_of_range> setDefaultHost(const std::string& host){

                if(host.empty())
                    return std::unexpected(std::out_of_range("expected host value parameter to contain a non-empty string"));

                if(host.size() > MAXFIELDVALUESIZE)
                    return std::unexpected(std::out_of_range("host value exceeds maximum header field length of 256"));

                defaultHost = host;
                return {};
            }

            //TODO: validate target for syntax or disallowed characters
            std::expected<Response, NinError> GET(const std::string& target){
                //TODO: move to request builder and research for header architecture
                //Use string views and spans for interrfaces
                
                internal::httpRequestBuilder builder;

                builder.setTarget(target);
                builder.setHost(defaultHost);

                auto& request = builder.get();
                auto requestStr = request.toString();

                if(auto sent = streamSock_.sendAll(std::span<const char>{requestStr.data(), requestStr.size()}); !sent.has_value())
                    return std::unexpected{NinError::fromSocketError(sent.error())};

                return parseResponse(streamSock_);
            }

        private:
            StreamSocket<EndpointT> streamSock_;

            std::expected<Response, NinError> parseResponse(StreamSocket<EndpointT>& sock){
                internal::httpResponseParser parser;
                std::string got;

                std::array<char, 512> buf{};

                auto htppParseStatus = internal::httpParseStatus::NeedData;
                do{
                    auto res = sock.receive(buf);

                    std::clog << "[http.client] receive returned\n";

                    if(!res.has_value()){
                        const SocketError& err = res.error();
                        if(err.category() == SocketErrorCategory::Interrupted)
                            continue;

                        if(err.category() == SocketErrorCategory::ConnectionClosed)
                            return std::unexpected{NinError::fromSocketCategory(SocketErrorCategory::ConnectionClosed, "Connection closed before a complete response was received")};

                        return std::unexpected{NinError::fromSocketError(err)};
                    }

                    size_t read = res.value();

                    if(read == 0){
                        std::clog << "[http.client] sender sent 0\n";
                        return std::unexpected{NinError::fromSocketCategory(SocketErrorCategory::ConnectionClosed, "Connection closed before a complete response was received")};
                    }

                    got.append(buf.data(), read);

                    std::clog << "[http.client] received " << got.size() << " bytes:\n" << got << '\n';

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

        private:
            std::string defaultHost;

            static constexpr const int MAXFIELDVALUESIZE = 256;
    };
} // namespace ninttp
