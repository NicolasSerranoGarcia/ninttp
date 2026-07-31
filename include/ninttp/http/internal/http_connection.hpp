#pragma once

#include <array>
#include <concepts>
#include <cstddef>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include "../../endpoints.hpp"
#include "../../error/nin_error.hpp"
#include "../../socket/error/socket_error_category.hpp"
#include "../../socket/socket.hpp"
#include "../http_limits.hpp"
#include "../types.hpp"
#include "http_request_parser.hpp"

namespace ninttp::internal
{
    enum class ConnectionLifecycle{
        Open,
        ClosingAfterWrite,
        Closed
    };

    enum class ConnectionExchangePhase{
        ReadingRequest,
        RequestReady,
        AwaitingResponse,
        WritingResponse
    };

    struct ConnectionInterests{
        bool read = false;
        bool write = false;

        constexpr bool operator==(const ConnectionInterests&) const noexcept = default;
    };

    // Internal, scheduler-neutral state for one accepted HTTP/1.x TCP connection.
    template<
        httpVersion ver = http_1_0,
        typename EndpointT = IPv4Endpoint,
        typename StreamT = StreamSocket<EndpointT>>
    class httpServerTCPConnection{
        static_assert(isSupportedHTTP1Version(ver),
            "HTTP server connections only support HTTP/1.0 and HTTP/1.1");
        static_assert(std::same_as<EndpointT, IPv4Endpoint> || std::same_as<EndpointT, IPv6Endpoint>,
            "HTTP server connections only accept IPv4 or IPv6 endpoints");

        public:
            explicit httpServerTCPConnection(StreamT stream) noexcept
                : stream_(std::move(stream)){}

            httpServerTCPConnection(const httpServerTCPConnection&) = delete;
            httpServerTCPConnection& operator=(const httpServerTCPConnection&) = delete;
            httpServerTCPConnection(httpServerTCPConnection&&) noexcept = default;
            httpServerTCPConnection& operator=(httpServerTCPConnection&&) noexcept = default;

            [[nodiscard]] ConnectionInterests interests() const noexcept{
                return ConnectionInterests{
                    .read = lifecycle_ == ConnectionLifecycle::Open &&
                        phase_ == ConnectionExchangePhase::ReadingRequest,
                    .write = lifecycle_ != ConnectionLifecycle::Closed && hasPendingOutput()
                };
            }

            [[nodiscard]] ConnectionLifecycle lifecycle() const noexcept{
                return lifecycle_;
            }

            [[nodiscard]] ConnectionExchangePhase exchangePhase() const noexcept{
                return phase_;
            }

            [[nodiscard]] bool closed() const noexcept{
                return lifecycle_ == ConnectionLifecycle::Closed;
            }

            [[nodiscard]] bool hasRequest() const noexcept{
                return phase_ == ConnectionExchangePhase::RequestReady &&
                    completedRequest_.has_value();
            }

            [[nodiscard]] std::optional<Request> takeRequest() noexcept{
                if(!hasRequest())
                    return std::nullopt;

                auto request = std::move(completedRequest_);
                completedRequest_.reset();
                phase_ = ConnectionExchangePhase::AwaitingResponse;
                return request;
            }

            [[nodiscard]] std::size_t pendingOutputBytes() const noexcept{
                return outputBuf_.size() - outputOffset_;
            }

            // Queues one streaming segment. finishResponse() marks the final segment.
            [[nodiscard]] bool queueOutput(std::string_view bytes){
                if(lifecycle_ == ConnectionLifecycle::Closed || responseFinished_ ||
                    (phase_ != ConnectionExchangePhase::AwaitingResponse &&
                        phase_ != ConnectionExchangePhase::WritingResponse))
                    return false;

                outputBuf_.append(bytes);
                phase_ = ConnectionExchangePhase::WritingResponse;
                return true;
            }

            // A serialized Response is a complete response, unlike raw streaming output.
            [[nodiscard]] bool queueResponse(const Response& response){
                if(lifecycle_ == ConnectionLifecycle::Closed || responseFinished_ ||
                    phase_ != ConnectionExchangePhase::AwaitingResponse)
                    return false;

                outputBuf_.append(response.toString());
                responseFinished_ = true;
                phase_ = ConnectionExchangePhase::WritingResponse;
                return true;
            }

            [[nodiscard]] std::expected<void, NinError> finishResponse(){
                if(lifecycle_ == ConnectionLifecycle::Closed ||
                    phase_ != ConnectionExchangePhase::WritingResponse ||
                    responseFinished_)
                    return {};

                responseFinished_ = true;
                if(hasPendingOutput())
                    return {};

                return completeResponseExchange();
            }

            void closeAfterWrite() noexcept{
                if(lifecycle_ == ConnectionLifecycle::Closed)
                    return;

                lifecycle_ = hasPendingOutput()
                    ? ConnectionLifecycle::ClosingAfterWrite
                    : ConnectionLifecycle::Closed;
            }

            [[nodiscard]] std::expected<void, NinError> onReadable(){
                if(!interests().read)
                    return {};

                std::array<char, limits::ReadBufferSize> buffer{};

                while(interests().read){
                    auto received = stream_.receive(buffer);
                    if(!received.has_value()){
                        const auto category = received.error().category();
                        if(category == SocketErrorCategory::Blocks)
                            return {};
                        if(category == SocketErrorCategory::Interrupted)
                            continue;

                        lifecycle_ = ConnectionLifecycle::Closed;
                        return std::unexpected{NinError::fromSocketError(received.error())};
                    }

                    if(*received == 0){
                        lifecycle_ = ConnectionLifecycle::Closed;
                        return std::unexpected{NinError::fromSocketCategory(
                            SocketErrorCategory::ConnectionClosed,
                            "Connection closed before a complete request was received")};
                    }

                    std::string input(buffer.data(), *received);
                    auto processed = processInput(input);
                    if(!processed.has_value())
                        return std::unexpected{std::move(processed).error()};
                }

                return {};
            }

            [[nodiscard]] std::expected<void, NinError> onWritable(){
                if(lifecycle_ == ConnectionLifecycle::Closed)
                    return {};

                while(hasPendingOutput()){
                    const auto pending = std::span<const char>{
                        outputBuf_.data() + outputOffset_,
                        pendingOutputBytes()
                    };

                    auto sent = stream_.send(pending);
                    if(!sent.has_value()){
                        const auto category = sent.error().category();
                        if(category == SocketErrorCategory::Blocks)
                            return {};
                        if(category == SocketErrorCategory::Interrupted)
                            continue;

                        lifecycle_ = ConnectionLifecycle::Closed;
                        return std::unexpected{NinError::fromSocketError(sent.error())};
                    }

                    if(*sent == 0){
                        lifecycle_ = ConnectionLifecycle::Closed;
                        return std::unexpected{NinError::fromSocketCategory(
                            SocketErrorCategory::ConnectionLost,
                            "Sending response bytes made no progress")};
                    }

                    outputOffset_ += *sent;
                }

                outputBuf_.clear();
                outputOffset_ = 0;

                if(lifecycle_ == ConnectionLifecycle::ClosingAfterWrite){
                    lifecycle_ = ConnectionLifecycle::Closed;
                    return {};
                }

                if(responseFinished_)
                    return completeResponseExchange();

                return {};
            }

            [[nodiscard]] StreamT& stream() noexcept{
                return stream_;
            }

            [[nodiscard]] const StreamT& stream() const noexcept{
                return stream_;
            }

        private:
            [[nodiscard]] bool hasPendingOutput() const noexcept{
                return outputOffset_ < outputBuf_.size();
            }

            std::expected<void, NinError> processInput(const std::string& input){
                auto parsed = parser_.append(input);
                if(!parsed.has_value()){
                    lifecycle_ = ConnectionLifecycle::ClosingAfterWrite;
                    phase_ = ConnectionExchangePhase::AwaitingResponse;
                    return std::unexpected{NinError::fromHttpParseError(parsed.error())};
                }

                if(*parsed != httpParseStatus::Done)
                    return {};

                completedRequest_.emplace(parser_.getRequest());
                pendingInput_ = parser_.getLeftoverBytes();
                parser_.reset();
                phase_ = ConnectionExchangePhase::RequestReady;
                return {};
            }

            std::expected<void, NinError> completeResponseExchange(){
                responseFinished_ = false;
                phase_ = ConnectionExchangePhase::ReadingRequest;

                if(pendingInput_.empty())
                    return {};

                std::string input = std::move(pendingInput_);
                pendingInput_.clear();
                return processInput(input);
            }

            StreamT stream_;
            ConnectionLifecycle lifecycle_ = ConnectionLifecycle::Open;
            ConnectionExchangePhase phase_ = ConnectionExchangePhase::ReadingRequest;
            httpRequestParser<ver> parser_;

            std::optional<Request> completedRequest_;
            std::string pendingInput_;

            std::string outputBuf_;
            std::size_t outputOffset_ = 0;
            bool responseFinished_ = false;
    };

    // Leave a backdoor for future connection transports.
    template<
        httpVersion ver = http_1_0,
        typename EndpointT = IPv4Endpoint,
        typename StreamT = StreamSocket<EndpointT>>
    using httpServerConnection = httpServerTCPConnection<ver, EndpointT, StreamT>;
} // namespace ninttp::internal
