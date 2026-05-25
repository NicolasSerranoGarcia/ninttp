#pragma once

#include "../socket/socket.hpp"
#include "../endpoints.hpp"
#include "../socket/types.hpp"
#include "internal/http_request_parser.hpp"
#include "internal/http_response_parser.hpp"
#include "internal/http_response_builder.hpp"
#include "types.hpp"

#include <vector>
#include <unordered_map>
#include <functional>
#include <expected>
#include <optional>
#include <utility>

namespace ninttp
{
    //1.0
    template<httpVersion ver = http_1_0, typename EndpointT = IPv4Endpoint>
    class httpServer{
        public:

            using GetHandlerT = std::function<void(Response&)>;
        
            httpServer()
                : listenerSock_(Domain::IPv4, Protocol::Tcp)
            {}

            std::optional<SocketError> listen(const EndpointT& interf){
                if(auto bindRes = listenerSock_.bind(interf); !bindRes.has_value())
                    return std::move(bindRes.error());

                if(auto listenRes = listenerSock_.listen(MAX_BACKLOG); !listenRes.has_value())
                    return std::move(listenRes.error());

                while(1){

                    //GHLT: at the very least the append method does parse the entirety of a request packet
                    internal::httpRequestParser parser;

                    auto acceptRes = listenerSock_.accept();
                    if(!acceptRes.has_value())
                        return std::move(acceptRes.error());

                    auto streamSock = std::move(acceptRes).value();

                    std::string got;

                    char buf[512];

                    while(!parser.finished()){
                        auto res = streamSock.receive(buf, sizeof(buf));

                        if(!res.has_value())
                            return std::move(res.error());

                        size_t read = res.value();

                        if(read == 0)
                            break;

                        for(int i = 0; i < read; ++i)
                            got.push_back(buf[i]);

                        //for now the dirtiest way is to pass got and then clear. It wastes a lot of time but just works
                        parser.append(got);
                        got.clear();
                    }

                    //assert getRequest leaves the internal Request defaulted
                    auto request = parser.getRequest();

                    //Depending on the request we might need to modify state, and at the end send the response

                    //we need to send the response. Essentially, we want to do a transformation of the Request into the response.
                    // The callback, if specified, for the resource that Response holds is the transformer of our request into the Response.
                    //this transformation is specified by the user, that specifies what to do depending on the resource.
                    //the interface should be 

                    //GHLT: we need to set the invariants about how the listener socket works with stream sockets. 
                    //do we use IPC or other threads when accepting a connection?

                    Response response;

                    //use contains bc we dont want to create the resource
                    //GHLT: this probably also triggers 404 or other depending on permissions
                    //the error message shall be returned to the client
                    if(getHandlers.contains(request.resource)){

                        //TODO allow only a handful of operations over the response object. For example, setting the contents,
                        //setting a header maybe? and so. The rest is constructed by the response builder
                        //this would be implemented by making the contents of response private, maybe friending the builders
                        //and creating two methods like setContent and setHeader(). For the moment the most reasonable is letting only 
                        //setContent because set headers might have many side effects and I don't know if it would be useful for the user
                        getHandlers[request.resource](response);

                        auto responseStr = internal::httpResponseBuilder::fromResponseObject(response);

                        //technically we are not finished with just one response. Even in 1.0 client can specify keepalive.
                        //we could use fork? threads? the thing is that we need to streams of execution from the point we create a stream socket.
                        streamSock.send(responseStr.data(), responseStr.size());
                    }
                }

                return std::nullopt;
            }

            void doGET(const std::string& resource, GetHandlerT callback){
                /*TODO: validate resource is not malicious and so*/
                getHandlers[resource] = callback;
            }

        private:
            //this is only thought for GET. We nee da reliable way to store the callbacks and so because not all methods need the same treatment
            std::unordered_map<std::string, GetHandlerT> getHandlers;
            ListenerSocket<EndpointT, StreamSocket<EndpointT>> listenerSock_;

            static constinit const int MAX_BACKLOG = 100;
    };

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
