#pragma once

#include "../socket/socket.hpp"
#include "../endpoints.hpp"
#include "../socket/types.hpp"

#include <vector>
#include <unordered_map>
#include <functional>
#include <optional>

namespace ninttp::internal{
    enum class HTTPVerb{
        GET,
        PUT,
        //DELETE,
        PATCH,
    };

    struct Header{
        std::string key;
        std::string value;
    };
} // namespace ninttp::internal

namespace ninttp
{

    struct Response{
        bool ok;
        std::optional<int> errCode;
        std::vector<internal::Header> headers;
        std::string contents;
        //...
    };

    struct Request{
        std::vector<internal::Header> headers;
        internal::HTTPVerb operation;
        std::string resource;
        //...
    };

    //1.0
    template<typename EndpointT = IPv4Endpoint>
    class httpServer{
        public:

            using HandlerT = std::function<void(Request, Response)>;
        
            httpServer()
                : listenerSock_(Domain::IPv4, Protocol::Tcp)
            {}

            std::expected<void, SocketError> listen(const EndpointT& interf){
                if(const auto bindRes = listenerSock_.bind(interf); !bindRes.has_value())
                    return std::unexpected{bindRes.error()};

                if(const auto listenRes = listenerSock_.listen(MAX_BACKLOG); !listenRes.has_value()){
                    return std::unexpected{listenRes.error()};
                }

                while(1){
                    auto acceptRes = listenerSock_.accept();
                    if(!acceptRes.has_value())
                        return std::unexpected{acceptRes.error()};

                    auto streamSock = std::move(acceptRes).value();
                    
                    std::string result;

                    char buf[512];

                    for(;;){
                        auto res = streamSock.receive(buf, sizeof(buf));

                        if(!res.has_value())
                            return std::unexpected{res.error()};

                        size_t read = res.value();

                        if(read == 0)
                            break;

                        for(int i = 0; i < read; ++i)
                            result.push_back(buf[i]);

                        if(result.ends_with("\r\n\r\n"))
                            break;
                    }

                    /*here res would represent the HTTP
                    request and we would have to process it*/
                    /*we could do that by encapsulating the info
                    in a Request struct, and together with the Response use it in the callback*/
                    //for now just return whatever the user sent us
                    streamSock.send(result.data(), result.size());
                }
            }

            void onGET(const std::string& resource, HandlerT callback){
                /*validate resource*/
                handlers[resource] = callback;
            }

        private:
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