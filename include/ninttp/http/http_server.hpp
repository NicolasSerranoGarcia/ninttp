#pragma once

#include <array>
#include <cassert>
#include <concepts>
#include <expected>
#include <functional>
#include <iostream>
#include <span>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "../endpoints.hpp"
#include "../error/nin_error.hpp"
#include "../socket/socket.hpp"
#include "../socket/traits.hpp"
#include "internal/http_request_parser.hpp"
#include "internal/parse_utils.hpp"
#include "types.hpp"
#include "internal/http_error_factory.hpp"

namespace ninttp
{
    template<httpVersion ver = http_1_0, typename EndpointT = IPv4Endpoint>
    class httpServer{
        static_assert(std::same_as<EndpointT, IPv4Endpoint> || std::same_as<EndpointT, IPv6Endpoint>,
            "HTTP server only accepts IPv4 or IPv6 endpoints");

        public:

            using HandlerT = std::function<void(Response&)>;

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

            [[nodiscard]] bool registerHost(std::string host){
                if(host.empty())
                    return false;

                return hosts_.try_emplace(std::move(host)).second;
            }

            [[nodiscard]] bool registerHandler(
                const std::string& host,
                const std::string& target,
                std::string method,
                HandlerT callback)
            {
                if(method.empty() || !callback)
                    return false;

                for(const char c : method){
                    if(!utils::isTChar(c))
                        return false;
                }

                auto hostIt = hosts_.find(host);
                if(hostIt == hosts_.end())
                    return false;

                registeredMethods_.insert(method);
                hostIt->second[target].insert_or_assign(std::move(method), std::move(callback));
                return true;
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
                        auto error = std::move(requestRes).error();
                        //TODO: works because type being socket guarantees optional is not empty
                        if(error.type == NinErrorType::Socket && *(error.socketCategory) == SocketErrorCategory::ConnectionClosed){
                            continue;
                        } else if(error.type == NinErrorType::Socket){
                            return std::unexpected{std::move(error)};
                        } else{ //httpParseError
                            assert(error.parseErrorType.has_value());

                            std::clog << "[http.server] request error: " << error.what << '\n';

                            auto errorResponse = internal::httpErrorFactory<ver>::fromParseErrorType(*error.parseErrorType);
                            if(auto sent = streamSock.sendAll(errorResponse); !sent.has_value())
                                return std::unexpected{NinError::fromSocketError(sent.error())};

                            continue;
                        }
                    }

                    request = std::move(*requestRes);

                    const auto hostIt = hosts_.find(request.headers.at("host"));
                    if(hostIt == hosts_.end()){
                        if(auto sent = sendStatus(streamSock, 421); !sent.has_value())
                            return std::unexpected{sent.error()};
                        continue;
                    }

                    if(!registeredMethods_.contains(request.method)){
                        if(auto sent = sendStatus(streamSock, 501); !sent.has_value())
                            return std::unexpected{sent.error()};
                        continue;
                    }

                    const auto targetIt = hostIt->second.find(request.target);
                    if(targetIt == hostIt->second.end()){
                        if(auto sent = sendStatus(streamSock, 404); !sent.has_value())
                            return std::unexpected{sent.error()};
                        continue;
                    }

                    const auto handlerIt = targetIt->second.find(request.method);
                    if(handlerIt == targetIt->second.end()){
                        if(auto sent = sendMethodNotAllowed(streamSock, targetIt->second); !sent.has_value())
                            return std::unexpected{sent.error()};
                        continue;
                    }

                    Response response;
                    handlerIt->second(response);

                    response.version = ver;
                    response.statusCode = 200;

                    if(response.body.has_value() && !response.body->empty()){
                        response.headers.push_back(internal::HeaderField{ .name = std::string("Content-Length"),
                                                                          .value = std::to_string(response.body->size())});
                    }

                    if(auto sent = streamSock.sendAll(response.toString()); !sent.has_value())
                        return std::unexpected{NinError::fromSocketError(sent.error())};
                }

                std::clog << "[http.server] listen loop exited\n";


                return {};
            }

        private:
            using MethodHandlers = std::unordered_map<std::string, HandlerT>;
            using Targets = std::unordered_map<std::string, MethodHandlers>;
            using Hosts = std::unordered_map<std::string, Targets>;

            std::expected<void, NinError> sendStatus(StreamSocket<EndpointT>& socket, StatusCode status){
                auto response = internal::httpErrorFactory<ver>::fromStatusCode(status);
                if(auto sent = socket.sendAll(response); !sent.has_value())
                    return std::unexpected{NinError::fromSocketError(sent.error())};

                return {};
            }

            std::expected<void, NinError> sendMethodNotAllowed(
                StreamSocket<EndpointT>& socket,
                const MethodHandlers& handlers)
            {
                Response response{ .version = ver, .statusCode = 405 };
                response.headers.push_back(internal::HeaderField{ .name = "Content-Length", .value = "0" });

                std::string allowed;
                for(const auto& [method, handler] : handlers){
                    if(!allowed.empty())
                        allowed += ", ";
                    allowed += method;
                }
                response.headers.push_back(internal::HeaderField{ .name = "Allow", .value = std::move(allowed) });

                if(auto sent = socket.sendAll(response.toString()); !sent.has_value())
                    return std::unexpected{NinError::fromSocketError(sent.error())};

                return {};
            }

            Hosts hosts_;
            std::unordered_set<std::string> registeredMethods_;
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
