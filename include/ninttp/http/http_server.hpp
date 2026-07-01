#pragma once

#include "../socket/socket.hpp"
#include "../endpoints.hpp"
#include "../socket/types.hpp"
#include "../nin_error.hpp"
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
#include <cassert>

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
             * @throws NinError with .type = Socket if listener socket construction fails.
             */
            httpServer() try
                : listenerSock_(Protocol::Tcp) {}
            catch(const SocketError& err) {
                throw NinError::fromSocketError(err);
            }

            //this signature is correct. the listener socket should only report failure if its own setup went wrong.
            std::expected<void, NinError> listen(const EndpointT& interf){
                if(auto bindRes = listenerSock_.bind(interf); !bindRes.has_value())
                    return std::unexpected{NinError::fromSocketError(bindRes.error())};

                if(auto listenRes = listenerSock_.listen(MAX_BACKLOG); !listenRes.has_value())
                    return std::unexpected{NinError::fromSocketError(listenRes.error())};

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
                        return std::unexpected{NinError::fromSocketError(acceptRes.error())};

                    clientSockets_.push_back(std::move(acceptRes).value());

                    auto& streamSock = clientSockets_[clientSockets_.size()-1];

                    Request request;
                    auto requestRes = parseConnection(streamSock);
                    if(!requestRes.has_value()){
                        std::clog << "[http.server] request error: " << requestRes.error().what << '\n';
                        continue;
                    }

                    request = std::move(*requestRes);

                    //Depending on the request we might need to modify state, and at the end send the response

                    //we need to send the response. Essentially, we want to do a transformation of the Request into the response.
                    // The callback, if specified, for the target that Response holds is the transformer of our request into the Response.
                    //this transformation is specified by the user, that specifies what to do depending on the target.
                    //the interface should be 

                    //GHLT: we need to set the invariants about how the listener socket works with stream sockets. 
                    //do we use IPC or other threads when accepting a connection?

                    Response response;

                    //use contains bc we dont want to create the target
                    //GHLT: this probably also triggers 404 or other depending on permissions
                    //the error message shall be returned to the client
                    if(getHandlers.contains(request.target)){

                        //TODO allow only a handful of operations over the response object. For example, setting the contents,
                        //setting a header maybe? and so. The rest is constructed by the response builder
                        //this would be implemented by making the contents of response private, maybe friending the builders
                        //and creating two methods like setContent and setHeader(). For the moment the most reasonable is letting only 
                        //setContent because set headers might have many side effects and I don't know if it would be useful for the user
                        getHandlers[request.target](response);

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
                            return std::unexpected{NinError::fromSocketError(sent.error())};
                    } else{
                        //send a 404
                    }
                }

                std::clog << "[http.server] listen loop exited\n";


                return {};
            }

            void doGET(const std::string& target, GetHandlerT callback){
                /*TODO: validate target is not malicious and so*/
                getHandlers[target] = callback;
            }

        private:
            //this is only thought for GET. We nee da reliable way to store the callbacks and so because not all methods need the same treatment
            std::unordered_map<std::string, GetHandlerT> getHandlers;
            ListenerSocket<EndpointT, StreamSocket<EndpointT>> listenerSock_;

            std::vector<StreamSocket<EndpointT>> clientSockets_;

            static constinit const int MAX_BACKLOG = 100;

            std::expected<Request, NinError> parseConnection(StreamSocket<EndpointT>& sock){
                internal::httpRequestParser parser;
                std::string got;

                std::array<char, 512> buf{};

                auto htppParseStatus = internal::httpParseStatus::NeedData;
                do{
                    auto res = sock.receive(buf);

                    if(!res.has_value()){
                        const SocketError& err = res.error();
                        if(err.category() == SocketErrorCategory::Interrupted)
                            continue;

                        if(err.category() == SocketErrorCategory::ConnectionClosed)
                            return std::unexpected{NinError::fromSocketCategory(SocketErrorCategory::ConnectionClosed, "Connection closed before a complete request was received")};

                        return std::unexpected{NinError::fromSocketError(err)};
                    }

                    size_t read = res.value();

                    if(read == 0){
                        std::clog << "[http.server] sender sent 0\n";
                        return std::unexpected{NinError::fromSocketCategory(SocketErrorCategory::ConnectionClosed, "Connection closed before a complete request was received")};
                    }

                    got.append(buf.data(), read);

                    std::clog << "[http.server] received " << got.size() << " bytes:\n" << got << '\n';
                    auto parseRes = parser.append(got);
                    got.clear();

                    if(!parseRes.has_value())
                        return std::unexpected{NinError::fromHttpParseError(parseRes.error())};

                    htppParseStatus = *parseRes;
                    if(htppParseStatus == internal::httpParseStatus::Done){
                        assert(parser.finished());
                        break;
                    }
                }while(htppParseStatus == internal::httpParseStatus::NeedData);

                std::clog << "[http.server] request parser finished\n";

                //assert getRequest leaves the internal Request defaulted
                return parser.getRequest();
            }
    };
} // namespace ninttp
