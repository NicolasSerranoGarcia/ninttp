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
            std::expected<Response, SocketError> GET(const std::string& resource){
                //TODO: move to request builder and research for header architecture
                //Use string views and spans for interrfaces
                std::string request = std::string("GET ") + resource + std::string(" ") +
                                    ver.toHeaderString() + std::string("\r\n\r\n");
                if(auto sent = streamSock_.sendAll(std::span<const char>{request.data(), request.size()}); !sent.has_value())
                    return std::unexpected{sent.error()};

                std::string got;

                std::array<char, 512> buf{};

                internal::httpResponseParser parser;

                auto htppParseStatus = internal::httpParseStatus::NeedData;
                do{
                    auto res = streamSock_.receive(buf);

                    std::clog << "[http.client] receive returned\n";
                    
                    if(!res.has_value())
                        return std::unexpected{res.error()};
                    
                        size_t read = res.value();

                        if(read == 0){
                            std::clog << "[http.client] sender sent 0\n";
                            break;
                        }

                    for(int i = 0; i < read; ++i)
                        got.push_back(buf[i]);

                    std::clog << "[http.client] received " << got.size() << " bytes:\n" << got << '\n';

                    auto parseRes = parser.append(got);
                    got.clear();

                    if(!parseRes.has_value()){
                        std::clog << parseRes.error().what << std::endl;
                        break;
                    }

                    htppParseStatus = *parseRes;
                    if(htppParseStatus == internal::httpParseStatus::Done){
                        assert(parser.finished());
                        break;
                    }
                }while(htppParseStatus == internal::httpParseStatus::NeedData);

                //here we would need to process the whole response
                return parser.getResponse();
            }

        private:
            StreamSocket<EndpointT> streamSock_;
    };
} // namespace ninttp
