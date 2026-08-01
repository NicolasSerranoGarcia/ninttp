#pragma once

#include <algorithm>
#include <cassert>
#include <concepts>
#include <expected>
#include <exception>
#include <functional>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "../endpoints.hpp"
#include "../error/nin_error.hpp"
#include "../socket/socket.hpp"
#include "../socket/traits.hpp"
#include "internal/http_error_factory.hpp"
#include "http_limits.hpp"
#include "internal/http_router.hpp"
#include "types.hpp"
#include "../socket/concepts.hpp"
#include "internal/http_connection.hpp"


namespace ninttp
{
    template<httpVersion ver = http_1_0, typename EndpointT = IPv4Endpoint>
    class httpServer{
        static_assert(isSupportedHTTP1Version(ver),
            "HTTP server only supports HTTP/1.0 and HTTP/1.1");
        static_assert(std::same_as<EndpointT, IPv4Endpoint> || std::same_as<EndpointT, IPv6Endpoint>,
            "HTTP server only accepts IPv4 or IPv6 endpoints");

        public:

            using HandlerT = std::function<void(const Request&, Response&)>;

            /**
             * @brief Construct a server with an opened listener socket.
             *
             * @throws NinError with .type = Socket if listener socket construction fails.
             */
            httpServer() try
                : listenerSock_(Protocol::Tcp) {
                    if constexpr(concepts::NonblockingConfigurable<decltype(listenerSock_)>){
                        if(auto set = listenerSock_.setNonblocking(true); !set)
                            throw set.error();
                    }
                }
            catch(const SocketError& err) {
                throw NinError::fromSocketError(err);
            }

            [[nodiscard]] bool registerHost(std::string host){
                return router_.registerHost(std::move(host));
            }

            [[nodiscard]] bool setDefaultHost(std::string host){
                return router_.setDefaultHost(std::move(host));
            }

            [[nodiscard]] bool registerHandler(
                const std::string& host,
                const std::string& target,
                std::string method,
                HandlerT callback)
            {
                return router_.registerHandler(host, target, std::move(method), std::move(callback));
            }

            //this signature is correct. the listener socket should only report failure if its own setup went wrong.
            std::expected<void, NinError> listen(const EndpointT& interf){
                if(auto bindRes = listenerSock_.bind(interf); !bindRes.has_value())
                    return std::unexpected{NinError::fromSocketError(bindRes.error())};

                if(auto listenRes = listenerSock_.listen(limits::MaxServerBacklog); !listenRes.has_value())
                    return std::unexpected{NinError::fromSocketError(listenRes.error())};

                
                while(true){
                    bool blocks = false;
                    auto acceptRes = listenerSock_.accept(blocks);
                    if(!acceptRes.has_value()){
                        auto err = acceptRes.error();
                        if(err.category() != SocketErrorCategory::Blocks && err.category() != SocketErrorCategory::Interrupted)
                            std::cerr << err.what() << std::endl;
                    } else{
                        clientConnections_.emplace_back(std::move(acceptRes).value());
                    }

                    for(auto& connection : clientConnections_){
                        auto readable = connection.onReadable();
                        if(!readable.has_value()){
                            handleConnectionError(connection, std::move(readable).error());
                            continue;
                        }

                        if(!connection.hasRequest())
                            continue;

                        auto request = connection.takeRequest();
                        assert(request.has_value());

                        auto result = router_.handleRequest(*request);

                        if(!result){
                            if(result.error() == 405){
                                if(auto sent = sendMethodNotAllowed(connection, router_.getAllowedMethods(*request)); !sent.has_value())
                                    handleConnectionError(connection, std::move(sent).error());
                            } else if(auto sent = sendStatus(connection, result.error()); !sent.has_value()){
                                handleConnectionError(connection, std::move(sent).error());
                            }
                            continue;
                        }

                        Response response;
                        try{
                            result->get()(*request, response);
                        } catch(const std::exception& error){
                            std::clog << "[http.server] request handler failed: " << error.what() << '\n';
                            if(auto sent = sendStatus(connection, 500); !sent.has_value())
                                handleConnectionError(connection, std::move(sent).error());
                            continue;
                        } catch(...){
                            std::clog << "[http.server] request handler failed with an unknown exception\n";
                            if(auto sent = sendStatus(connection, 500); !sent.has_value())
                                handleConnectionError(connection, std::move(sent).error());
                            continue;
                        }

                        response.setVersion(ver);
                        response.setStatusCode(200);
                        if(response.getBodyFraming() == ResponseBodyFraming::None)
                            response.clearContent();

                        const auto responseFraming = response.getBodyFraming();
                        const bool queued = connection.queueResponse(response);
                        assert(queued);
                        (void)queued;

                        if(responseFraming == ResponseBodyFraming::ConnectionClose)
                            connection.closeAfterWrite();

                        if(auto sent = connection.onWritable(); !sent.has_value())
                            handleConnectionError(connection, std::move(sent).error());
                    }

                    std::erase_if(clientConnections_, [](const auto& connection){
                        return connection.closed();
                    });
                }

                std::clog << "[http.server] listen loop exited\n";


                return {};
            }

        private:
            using ConnectionT = internal::httpServerConnection<ver, EndpointT>;

            void handleConnectionError(ConnectionT& connection, NinError error){
                if(error.type == NinErrorType::Socket){
                    const auto category = error.socketCategory.value_or(SocketErrorCategory::Other);

                    if(category == SocketErrorCategory::Blocks ||
                        category == SocketErrorCategory::Interrupted ||
                        category == SocketErrorCategory::ConnectionClosed)
                        return;

                    std::clog << "[http.server] connection error: " << error.what << '\n';
                    return;
                }

                std::clog << "[http.server] request error: " << error.what << '\n';

                if(!error.parseErrorType.has_value()){
                    connection.closeAfterWrite();
                    return;
                }

                auto errorResponse = internal::httpErrorFactory<ver>::fromParseErrorType(*error.parseErrorType);
                if(!connection.queueOutput(errorResponse)){
                    connection.closeAfterWrite();
                    return;
                }

                if(auto finished = connection.finishResponse(); !finished.has_value()){
                    std::clog << "[http.server] connection error while finishing error response: "
                              << finished.error().what << '\n';
                    connection.closeAfterWrite();
                    return;
                }

                connection.closeAfterWrite();
                if(auto sent = connection.onWritable(); !sent.has_value() &&
                    sent.error().socketCategory != SocketErrorCategory::ConnectionClosed)
                    std::clog << "[http.server] connection error while sending error response: "
                              << sent.error().what << '\n';
            }

            std::expected<void, NinError> sendStatus(ConnectionT& connection, StatusCode status){
                auto response = internal::httpErrorFactory<ver>::fromStatusCode(status);
                const bool queued = connection.queueOutput(response);
                assert(queued);
                (void)queued;
                if(auto finished = connection.finishResponse(); !finished.has_value())
                    return std::unexpected{std::move(finished).error()};
                if(auto sent = connection.onWritable(); !sent.has_value())
                    return std::unexpected{std::move(sent).error()};

                return {};
            }

            std::expected<void, NinError> sendMethodNotAllowed(
                ConnectionT& connection,
                std::string allowedMethods)
            {
                Response response{ver, 405};
                response.clearContent();
                response.addHeader("Allow", std::move(allowedMethods));

                const bool queued = connection.queueResponse(response);
                assert(queued);
                (void)queued;
                if(auto sent = connection.onWritable(); !sent.has_value())
                    return std::unexpected{std::move(sent).error()};

                return {};
            }

            ListenerSocket<EndpointT, StreamSocket<EndpointT>> listenerSock_;

            internal::httpRouter<ver> router_;

            std::vector<ConnectionT> clientConnections_;
    };
} // namespace ninttp
