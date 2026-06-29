#pragma once

#include "../socket/socket.hpp"
#include "../endpoints.hpp"
#include "../socket/types.hpp"
#include "internal/http_request_parser.hpp"
#include "types.hpp"

#include <array>
#include <concepts>
#include <vector>
#include <unordered_map>
#include <functional>
#include <expected>
#include <utility>
#include <iostream>

namespace ninttp
{
    //1.0
    template<httpVersion ver = http_1_0, typename EndpointT = IPv4Endpoint>
    class httpServer{
        static_assert(std::same_as<EndpointT, IPv4Endpoint> || std::same_as<EndpointT, IPv6Endpoint>,
            "HTTP server only accepts IPv4 or IPv6 endpoints");

        public:

            using GetHandlerT = std::function<void(Response&)>;
        
            /**
             * @brief Construct a server with an opened listener socket.
             *
             * @throws SocketError If listener socket construction fails.
             */
            httpServer()
                : listenerSock_(Protocol::Tcp)
            {}

            std::expected<void, SocketError> listen(const EndpointT& interf){
                if(auto bindRes = listenerSock_.bind(interf); !bindRes.has_value())
                    return std::unexpected{bindRes.error()};

                if(auto listenRes = listenerSock_.listen(MAX_BACKLOG); !listenRes.has_value())
                    return std::unexpected{listenRes.error()};

                while(1){

                    //parse saved sockets from previous clients before trying to accept
                    // for(auto& s : clientSockets_){
                    //     if(!s.isUsable()){
                    //         //probably dispose of it or handle it
                    //     }
                        
                    //     //here we would need to check if it blocks
                    //     //TODO: also, if they are not spread out into their own programs or threads, this would need a pq or ordering for priority of connections
                    //     //at the moment we will just receive some

                    // }

                    //GHLT: at the very least the append method does parse the entirety of a request packet
                    internal::httpRequestParser parser;

                    auto acceptRes = listenerSock_.accept();
                    if(!acceptRes.has_value())
                        return std::unexpected{std::move(acceptRes.error())};

                    clientSockets_.push_back(std::move(acceptRes).value());

                    auto& streamSock = clientSockets_[clientSockets_.size()-1];

                    std::string got;

                    std::array<char, 512> buf{};
                    bool parseFailed = false;

                    auto htppParseStatus = internal::httpParseStatus::NeedData;
                    do{
                        auto res = streamSock.receive(buf);

                        if(!res.has_value())
                            return std::unexpected{std::move(res.error())};

                        size_t read = res.value();

                        if(read == 0){
                            std::clog << "[http.server] sender sent 0\n";
                            break;
                        }

                        for(int i = 0; i < read; ++i)
                            got.push_back(buf[i]);

                        std::clog << "[http.server] received " << got.size() << " bytes:\n" << got << '\n';
                        //for now the dirtiest way is to pass got and then clear. It wastes a lot of time but just works
                        auto parseRes = parser.append(got);
                        got.clear();

                        if(!parseRes.has_value()){
                            //TODO: when all of this parsing loop is moved into its own function, this will return a parseError. This should be used 
                            //for something. Used or not, moving this to its function removes the need for the parseFailed variable, because if the function
                            //fails it will return the error. Pass a ref to the socket on the function and parse inside the function. In the future 
                            //it will just get called into another thread or process. 
                            std::clog << "[http.server] request parse error: " << parseRes.error().what << '\n';
                            parseFailed = true;
                            break;
                        }

                        htppParseStatus = *parseRes;
                        if(htppParseStatus == internal::httpParseStatus::Done){
                            assert(parser.finished());
                            break;
                        }
                    }while(htppParseStatus == internal::httpParseStatus::NeedData);

                    if(parseFailed){
                        //we could maybe check for streamSock state
                        continue;
                    }

                    std::clog << "[http.server] request parser finished\n";

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

                        response.version = ver;
                        response.statusCode = 200;

                        if(response.body.has_value() && !response.body->empty()){
                            response.headers.push_back(internal::Header{ .key = std::string("Content-Length"), 
                                                                         .value = std::to_string(response.body->size())});
                        }

                        auto responseStr = response.toString();

                        //technically we are not finished with just one response. Even in 1.0 client can specify keepalive.
                        //we could use fork? threads? the thing is that we need to streams of execution from the point we create a stream socket.
                        if(auto sent = streamSock.sendAll(std::span<const char>{responseStr.data(), responseStr.size()}); !sent.has_value())
                            return std::unexpected{std::move(sent.error())};
                    } else{
                        //send a 404
                    }
                }

                std::clog << "[http.server] listen loop exited\n";


                return {};
            }

            void doGET(const std::string& resource, GetHandlerT callback){
                /*TODO: validate resource is not malicious and so*/
                getHandlers[resource] = callback;
            }

        private:
            //this is only thought for GET. We nee da reliable way to store the callbacks and so because not all methods need the same treatment
            std::unordered_map<std::string, GetHandlerT> getHandlers;
            ListenerSocket<EndpointT, StreamSocket<EndpointT>> listenerSock_;

            std::vector<StreamSocket<EndpointT>> clientSockets_;

            static constinit const int MAX_BACKLOG = 100;
    };
} // namespace ninttp
