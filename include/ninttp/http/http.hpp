#pragma once

#include "../socket/socket.hpp"
#include "../socket/endpoints.hpp"
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
                : list_(Domain::IPv4, Protocol::Tcp)
            {}

            std::expected<void, SocketError> listen(const EndpointT& interf){
                if(const auto res = list_.bind(interf); !res.has_value())
                    return std::unexpected{res.error()};

                list_.listen(100);

                while(1){
                    auto sock = list_.accept();
                    if(!sock.has_value())
                        return std::unexpected{sock.error()};

                    auto streamSock = std::move(sock).value();
                    
                    std::string result;
                    result.reserve(512);

                    char buf[512];

                    for(;;){
                        std::memset(buf, 0, 512);
                        auto res = streamSock.receive(buf, 
                                    sizeof(buf));
                        if(!res.has_value())
                            return std::unexpected{res.error()};

                        int i = 0;

                        while(buf[i] != '\0'){
                            result.push_back(buf[i]);
                            ++i;
                        }

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
            ListenerSocket<EndpointT, StreamSocket<EndpointT>> list_;
    };

    template<typename EndpointT = IPv4Endpoint>
    class httpClient{
        public:

            httpClient() = delete;

            httpClient(const EndpointT& peer)
                : sock_(Domain::IPv4, Protocol::Tcp)
            {
                if(const auto res = sock_.connect(peer); !res.has_value())
                    throw res.error();
            }

            //at the moment literally send the resource
            std::expected<Response, SocketError> GET(const std::string& resource){
                std::string msg = resource + std::string("\r\n\r\n");
                sock_.send(msg.data(), msg.size());

                Response response;

                char buf[512];

                for(;;){
                    std::memset(buf, 0, 512);
                    auto res = sock_.receive(buf, sizeof(buf));
                    if(!res.has_value())
                        return std::unexpected{res.error()};

                    int i = 0;

                    while(buf[i] != '\0'){
                        response.contents.push_back(buf[i]);
                        ++i;
                    }

                    if(response.contents.ends_with("\r\n\r\n"))
                        break;
                }

                //here we would need to process the whole response
                return response;
            }

        private:
            StreamSocket<EndpointT> sock_;
    };
} // namespace ninttp