#pragma once

#include "../socket/socket.hpp"
#include "../endpoints.hpp"
#include "../socket/types.hpp"
#include "internal/http_response_parser.hpp"
#include "internal/http_request_builder.hpp"
#include "types.hpp"

#include <array>
#include <concepts>
#include <vector>
#include <expected>
#include <utility>
#include <iostream>
#include <cassert>

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
             * @throws SocketError If stream socket construction or connection fails.
             */
            httpClient(const EndpointT& peer)
                : streamSock_(Protocol::Tcp)
            {
                if(const auto res = streamSock_.connect(peer); !res.has_value())
                    throw res.error();
            }

            //TODO: validate resource for syntax or disallowed characters
            std::expected<Response, internal::httpParseError> GET(const std::string& resource){
                //TODO: move to request builder and research for header architecture
                //Use string views and spans for interrfaces
                std::string request = std::string("GET ") + resource + std::string(" ") +
                                    ver.toHeaderString() + std::string("\r\n\r\n");
                if(auto sent = streamSock_.sendAll(std::span<const char>{request.data(), request.size()}); !sent.has_value())
                    throw sent.error();

                //this should not throw if a socket operation fails, but idk how to manage this because there are two semantic operations being carried:
                //parsing and socketing. Any of the two might fail. morph one into the other? loosen restricitions on any of each and make a more general error for http that is not
                //actual error codes returned from responses?
                return parseResponse(streamSock_);
            }

        private:
            StreamSocket<EndpointT> streamSock_;

            //throws SocketError for socket failures that are not an orderly close
            std::expected<Response, internal::httpParseError> parseResponse(StreamSocket<EndpointT>& sock){
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
                            return std::unexpected{internal::httpParseError{ .what = "Connection closed before a complete response was received" }};

                        throw err;
                    }

                    size_t read = res.value();

                    if(read == 0){
                        std::clog << "[http.client] sender sent 0\n";
                        return std::unexpected{internal::httpParseError{ .what = "Connection closed before a complete response was received" }};
                    }

                    got.append(buf.data(), read);

                    std::clog << "[http.client] received " << got.size() << " bytes:\n" << got << '\n';

                    auto parseRes = parser.append(got);
                    got.clear();

                    if(!parseRes.has_value())
                        return std::unexpected{parseRes.error()};

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
