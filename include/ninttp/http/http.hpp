#pragma once

#include "../socket/socket.hpp"
#include "../endpoints.hpp"
#include "../socket/types.hpp"
#include "internal/http_request_parser.hpp"
#include "internal/http_response_builder.hpp"
#include "types.hpp"

#include <vector>
#include <unordered_map>
#include <functional>
#include <expected>

namespace ninttp
{
    //1.0
    template<httpVersion ver = http_1_0, typename EndpointT = IPv4Endpoint>
    class httpServer{
        public:

            using HandlerT = std::function<void(Request, Response&)>;
        
            httpServer()
                : listenerSock_(Domain::IPv4, Protocol::Tcp)
            {}

            std::expected<void, SocketError> listen(const EndpointT& interf){
                if(const auto bindRes = listenerSock_.bind(interf); !bindRes.has_value())
                    return std::unexpected{bindRes.error()};

                if(const auto listenRes = listenerSock_.listen(MAX_BACKLOG); !listenRes.has_value()){
                    return std::unexpected{listenRes.error()};
                }

                internal::httpRequestParser parser;

                while(1){
                    auto acceptRes = listenerSock_.accept();
                    if(!acceptRes.has_value())
                        return std::unexpected{acceptRes.error()};

                    auto streamSock = std::move(acceptRes).value();

                    std::string got;

                    char buf[512];

                    while(!parser.finished()){
                        auto res = streamSock.receive(buf, sizeof(buf));

                        if(!res.has_value())
                            return std::unexpected{res.error()};

                        size_t read = res.value();

                        if(read == 0)
                            break;

                        for(int i = 0; i < read; ++i)
                            got.push_back(buf[i]);

                        //for now the dirtiest way is to pass got and then clear. It wastes a lot of time but just works
                        parser.append(got);
                        got.clear();
                    }

                    auto request = parser.getRequest();

                    //we need to send the response. Essentially, we want to do a transformation of the Request into the response.
                    // The callback, if specified, for the resource that Response holds is the transformer of our request into the Response.
                    //this transformation is specified by the user, that specifies what to do depending on the resource.
                    //the interface should be 

                    Response response;

                    //use contains bc we dont want to create the resource
                    if(handlers.contains(request.resource))
                        handlers[request.resource](std::move(request), response);

                    auto responseStr = internal::httpResponseBuilder::fromResponseObject(response);

                    //technically we are not finished with just one response. Even in 1.0 client can specify keepalive.
                    //we could use fork? threads? the thing is that we need to streams of execution from the point we create a stream socket.
                    streamSock.send(responseStr.data(), responseStr.size());
                }
            }

            void doGET(const std::string& resource, HandlerT callback){
                /*TODO: validate resource*/
                handlers[resource] = callback;
            }

        private:
            //this is only thought for GET. We nee da reliable way to store the callbacks and so because not all methods need the same treatment
            std::unordered_map<std::string, HandlerT> handlers;
            ListenerSocket<EndpointT, StreamSocket<EndpointT>> listenerSock_;

            static constinit const int MAX_BACKLOG = 100;
    };

    template<typename EndpointT = IPv4Endpoint>
    class httpClient{
        public:

            httpClient() = delete;

            httpClient(const EndpointT& peer)
                : streamSock_(Domain::IPv4, Protocol::Tcp)
            {
                if(const auto res = streamSock_.connect(peer); !res.has_value())
                    throw res.error();
            }

            //at the moment literally send the resource
            std::expected<Response, SocketError> GET(const std::string& resource){
                std::string msg = resource + std::string("\r\n\r\n");
                streamSock_.send(msg.data(), msg.size());

                Response response;

                char buf[512];

                for(;;){
                    auto res = streamSock_.receive(buf, sizeof(buf));
                    if(!res.has_value())
                        return std::unexpected{res.error()};

                    size_t got = res.value();

                    if(got == 0)
                        break;

                    for(size_t i = 0; i < got; ++i)
                        response.contents.push_back(buf[i]);

                    if(response.contents.ends_with("\r\n\r\n"))
                        break;
                }

                //here we would need to process the whole response
                return response;
            }

        private:
            StreamSocket<EndpointT> streamSock_;
    };
} // namespace ninttp