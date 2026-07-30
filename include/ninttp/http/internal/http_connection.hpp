#pragma once

#include <string>

#include "../../socket/socket.hpp"
#include "../../endpoints.hpp"
#include "http_request_parser.hpp"
#include "http_response_parser.hpp"
#include "../types.hpp"

namespace ninttp::internal{

    enum class ConnectionState{
        Readable,
        Writable,
        Idle,
        Closed
    };

    //internal use
    template <typename EndpointT = IPv4Endpoint>
    class httpServerTCPConnection{
        static_assert(isSupportedHTTP1Version(ver),
            "HTTP server only supports HTTP/1.0 and HTTP/1.1");

        static_assert(std::is_same<EndpointT, IPv4Endpoint> || std::is_same<EndpointT, IPv6Endpoint>, 
            "HTTP server only accepts IPv4 or IPv6 endpoints");

            std::expected<void, NinError> trySendResponse(const Response& response){
                if(state_ != ConnectionState::Writable){
                    
                }


            }

        private:
            StreamSocket<EndpointT> stream_;
            ConnectionState state_;
            httpRequestParser parser_;

            std::string outputBuf_;
            std::size outputOffset_;
    };

    //leave a backdoor for future connection types
    template <typename EndpointT = IPv4Endpoint>
    using httpServerConnection = httpServerTCPConnection<EndpointT>;
}