#pragma once

#include "../socket/socket.hpp"
#include "../endpoints.hpp"
#include "../socket/types.hpp"
#include "internal/http_response_parser.hpp"
#include "internal/http_request_builder.hpp"
#include "types.hpp"

#include <vector>
#include <expected>
#include <utility>
#include <iostream>

namespace ninttp
{
    template<httpVersion ver = http_1_0, typename EndpointT = IPv4Endpoint>
    class httpClient{
        public:

            httpClient() = delete;

            httpClient(const EndpointT& peer)
                : streamSock_(Domain::IPv4, Protocol::Tcp)
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
                streamSock_.send(request.data(), request.size());

                std::string got;

                char buf[512];

                internal::httpResponseParser parser;

                while(!parser.finished()){
                    auto res = streamSock_.receive(buf, sizeof(buf));

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

                    parser.append(got);
                    got.clear();
                }

                //here we would need to process the whole response
                return parser.getResponse();
            }

        private:
            StreamSocket<EndpointT> streamSock_;
    };
} // namespace ninttp
