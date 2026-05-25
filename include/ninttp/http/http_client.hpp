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
                std::string request = std::string("GET ") + resource + std::string(" ") +
                                    ver.toHeaderString() + std::string("\r\nContent-Length: ") + 
                                    std::to_string(resource.size()) + std::string("\r\n\r\n") + resource;
                streamSock_.send(request.data(), request.size());

                std::string got;

                char buf[512];

                internal::httpResponseParser parser;

                
                while(!parser.finished()){
                    auto res = streamSock_.receive(buf, sizeof(buf));
                    
                    if(!res.has_value())
                        return std::unexpected{res.error()};
                    
                    size_t read = res.value();
                        
                    if(read == 0)
                        break;

                    std::cout << "Hola" << std::endl;

                    for(int i = 0; i < read; ++i)
                        got.push_back(buf[i]);

                    std::cout << got << std::endl;

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
