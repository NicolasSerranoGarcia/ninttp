#include <algorithm>
#include <cassert>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <expected>
#include <span>
#include <string>
#include <utility>

#include "ninttp/http/internal/http_connection.hpp"

namespace
{
    class FakeStream{
        public:
            explicit FakeStream(std::string input = {})
                : input_(std::move(input)){}

            std::expected<std::size_t, ninttp::SocketError> receive(std::span<char> buffer){
                if(input_.empty() && blockWhenEmpty_){
#if defined(_WIN32)
                    return std::unexpected{ninttp::SocketError{WSAEWOULDBLOCK}};
#else
                    return std::unexpected{ninttp::SocketError{EAGAIN}};
#endif
                }

                const auto count = std::min(buffer.size(), input_.size());
                std::copy_n(input_.data(), count, buffer.data());
                input_.erase(0, count);
                return count;
            }

            std::expected<std::size_t, ninttp::SocketError> send(std::span<const char> bytes){
                const auto count = std::min(bytes.size(), MaxWriteSize);
                output_.append(bytes.data(), count);
                return count;
            }

            [[nodiscard]] const std::string& output() const noexcept{
                return output_;
            }

            std::expected<void, ninttp::SocketError> shutdown(ninttp::ShutdownPolicy policy){
                assert(policy == ninttp::ShutdownPolicy::SHUT_TRANSMISSIONS);
                transmissionShutdown_ = true;
                return {};
            }

            void blockWhenEmpty(bool set) noexcept{
                blockWhenEmpty_ = set;
            }

            [[nodiscard]] bool transmissionShutdown() const noexcept{
                return transmissionShutdown_;
            }

        private:
            static constexpr std::size_t MaxWriteSize = 5;

            std::string input_;
            std::string output_;
            bool blockWhenEmpty_ = false;
            bool transmissionShutdown_ = false;
    };
}

int main(){
    using Connection = ninttp::internal::httpServerConnection<
        ninttp::http_1_1,
        ninttp::IPv4Endpoint,
        FakeStream>;
    using Interests = ninttp::internal::ConnectionInterests;
    using Lifecycle = ninttp::internal::ConnectionLifecycle;
    using Phase = ninttp::internal::ConnectionExchangePhase;
    using Timeout = ninttp::internal::ConnectionTimeoutType;

    {
        FakeStream stream{
            "GET /one HTTP/1.1\r\nHost: example.test\r\n\r\n"
            "GET /two HTTP/1.1\r\nHost: example.test\r\n\r\n"};
        Connection connection{std::move(stream)};

        assert((connection.interests() == Interests{.read = true, .write = false}));
        assert(connection.exchangePhase() == Phase::ReadingRequest);
        assert(connection.onReadable().has_value());

        assert(connection.hasRequest());
        assert(connection.exchangePhase() == Phase::RequestReady);
        assert(connection.interests() == Interests{});

        auto firstRequest = connection.takeRequest();
        assert(firstRequest.has_value());
        assert(firstRequest->getTarget() == "/one");
        assert(connection.exchangePhase() == Phase::AwaitingResponse);

        ninttp::Response response{ninttp::http_1_1, 200};
        assert(response.setContent("first"));
        assert(connection.queueResponse(response));
        assert(connection.exchangePhase() == Phase::WritingResponse);
        assert((connection.interests() == Interests{.read = false, .write = true}));

        assert(connection.onWritable().has_value());
        assert(connection.stream().output() == response.toString());

        // The pipelined bytes are parsed only after response one is complete.
        assert(connection.hasRequest());
        assert(connection.exchangePhase() == Phase::RequestReady);

        auto secondRequest = connection.takeRequest();
        assert(secondRequest.has_value());
        assert(secondRequest->getTarget() == "/two");

        // Raw output remains an open stream until finishResponse() is called.
        assert(connection.queueOutput("first segment"));
        assert(connection.onWritable().has_value());
        assert(connection.exchangePhase() == Phase::WritingResponse);
        assert(connection.interests() == Interests{});

        assert(connection.queueOutput("second segment"));
        assert(connection.onWritable().has_value());
        assert(connection.finishResponse().has_value());
        assert(connection.exchangePhase() == Phase::ReadingRequest);
        assert((connection.interests() == Interests{.read = true, .write = false}));
    }

    {
        Connection connection{FakeStream{}};

        assert(connection.lifecycle() == Lifecycle::Open);
        connection.closeAfterWrite();
        assert(connection.lifecycle() == Lifecycle::ClosingAfterWrite);
        assert((connection.interests() == Interests{.read = false, .write = true}));
        assert(!connection.queueOutput("ignored"));
        assert(connection.onWritable().has_value());
        assert(connection.lifecycle() == Lifecycle::DrainingPeer);
        assert(connection.stream().transmissionShutdown());
        assert((connection.interests() == Interests{.read = true, .write = false}));
        assert(connection.onReadable().has_value());
        assert(connection.closed());
    }

    {
        FakeStream stream{"GET / HTTP/1.1\r\nHost: example.test\r\n\r\n"};
        Connection connection{std::move(stream)};
        assert(connection.onReadable().has_value());
        assert(connection.requestWillPersist());
        assert(connection.takeRequest().has_value());

        ninttp::Response response{ninttp::http_1_1, 200};
        response.clearContent();
        assert(connection.queueResponse(response));
        assert(connection.onWritable().has_value());
        assert(connection.lifecycle() == Lifecycle::Open);
        assert(connection.stream().output().find("Connection:") == std::string::npos);
    }

    {
        FakeStream stream{
            "GET / HTTP/1.1\r\n"
            "Host: example.test\r\n"
            "Connection: keep-alive, CLOSE\r\n\r\n"};
        Connection connection{std::move(stream)};
        assert(connection.onReadable().has_value());
        assert(!connection.requestWillPersist());
        assert(connection.takeRequest().has_value());

        ninttp::Response response{ninttp::http_1_1, 200};
        response.clearContent();
        assert(connection.queueResponse(response));
        assert(connection.onWritable().has_value());
        assert(connection.lifecycle() == Lifecycle::DrainingPeer);
        assert(connection.stream().output().find("Connection: close\r\n") != std::string::npos);
    }

    {
        FakeStream stream{
            "GET / HTTP/1.0\r\n"
            "Connection: Keep-Alive\r\n\r\n"};
        Connection connection{std::move(stream)};
        assert(connection.onReadable().has_value());
        assert(connection.requestWillPersist());
        assert(connection.takeRequest().has_value());

        ninttp::Response response{ninttp::http_1_0, 200};
        response.clearContent();
        assert(connection.queueResponse(response));
        assert(connection.onWritable().has_value());
        assert(connection.lifecycle() == Lifecycle::Open);
        assert(connection.stream().output().find("Connection: keep-alive\r\n") != std::string::npos);
    }

    {
        ninttp::httpConnectionPolicy policy;
        policy.allowHttp10KeepAlive = false;
        Connection connection{
            FakeStream{"GET / HTTP/1.0\r\nConnection: keep-alive\r\n\r\n"},
            policy};
        assert(connection.onReadable().has_value());
        assert(!connection.requestWillPersist());
    }

    {
        ninttp::httpConnectionPolicy policy;
        policy.pipelining = ninttp::httpPipeliningPolicy::Disabled;
        Connection connection{
            FakeStream{
                "GET /one HTTP/1.1\r\nHost: example.test\r\n\r\n"
                "GET /two HTTP/1.1\r\nHost: example.test\r\n\r\n"},
            policy};
        assert(connection.onReadable().has_value());
        assert(!connection.requestWillPersist());
        assert(connection.requestsReceived() == 1);
    }

    {
        ninttp::httpConnectionPolicy policy;
        policy.maxPipelinedBytes = 1;
        Connection connection{
            FakeStream{
                "GET /one HTTP/1.1\r\nHost: example.test\r\n\r\n"
                "GET /two HTTP/1.1\r\nHost: example.test\r\n\r\n"},
            policy};
        assert(connection.onReadable().has_value());
        assert(!connection.requestWillPersist());
    }

    {
        ninttp::httpConnectionPolicy policy;
        policy.maxRequests = 1;
        Connection connection{
            FakeStream{"GET / HTTP/1.1\r\nHost: example.test\r\n\r\n"},
            policy};
        assert(connection.onReadable().has_value());
        assert(connection.requestsReceived() == 1);
        assert(!connection.requestWillPersist());
    }

    {
        Connection connection{
            FakeStream{"GET / HTTP/1.1\r\nHost: example.test\r\n\r\n"}};
        assert(connection.onReadable().has_value());
        assert(connection.takeRequest().has_value());

        ninttp::Response response{ninttp::http_1_1, 200};
        response.clearContent();
        assert(response.setHeader("Connection", "close"));
        assert(connection.queueResponse(response));
        assert(connection.onWritable().has_value());
        assert(connection.lifecycle() == Lifecycle::DrainingPeer);
    }

    {
        Connection connection{
            FakeStream{"GET / HTTP/1.1\r\nBroken header\r\n\r\n"}};
        assert(!connection.onReadable().has_value());

        ninttp::Response response{ninttp::http_1_1, 400};
        response.clearContent();
        assert(connection.queueResponse(response));
        assert(connection.onWritable().has_value());
        assert(connection.stream().output().find("Connection: close\r\n") != std::string::npos);
        assert(connection.lifecycle() == Lifecycle::DrainingPeer);
    }

    {
        ninttp::httpConnectionPolicy policy;
        policy.idleTimeout = std::chrono::milliseconds{10};
        const auto start = Connection::Clock::now();
        Connection connection{FakeStream{}, policy, start};
        const auto idleDeadline = connection.deadline();
        assert(idleDeadline == start + policy.idleTimeout);
        assert(connection.expiredTimeout(*idleDeadline) == Timeout::Idle);
        assert(connection.onTimeout(*idleDeadline).value());
        assert(connection.lifecycle() == Lifecycle::DrainingPeer);
    }

    {
        ninttp::httpConnectionPolicy policy;
        policy.incompleteRequestTimeout = std::chrono::milliseconds{10};
        FakeStream stream{"GET /"};
        stream.blockWhenEmpty(true);
        Connection connection{std::move(stream), policy};
        assert(connection.onReadable().has_value());
        const auto incompleteDeadline = connection.deadline();
        assert(incompleteDeadline.has_value());
        assert(connection.expiredTimeout(*incompleteDeadline) == Timeout::IncompleteRequest);
    }

    {
        ninttp::httpConnectionPolicy policy;
        policy.responseTimeout = std::chrono::milliseconds{10};
        Connection connection{
            FakeStream{"GET / HTTP/1.1\r\nHost: example.test\r\n\r\n"},
            policy};
        assert(connection.onReadable().has_value());
        const auto responseDeadline = connection.deadline();
        assert(responseDeadline.has_value());
        assert(connection.expiredTimeout(*responseDeadline) == Timeout::Response);
    }

    {
        ninttp::httpConnectionPolicy policy;
        policy.writeTimeout = std::chrono::milliseconds{10};
        Connection connection{
            FakeStream{"GET / HTTP/1.1\r\nHost: example.test\r\n\r\n"},
            policy};
        assert(connection.onReadable().has_value());
        assert(connection.takeRequest().has_value());
        assert(connection.queueOutput("pending"));
        const auto writeDeadline = connection.deadline();
        assert(writeDeadline.has_value());
        assert(connection.expiredTimeout(*writeDeadline) == Timeout::Write);
    }

    {
        ninttp::httpConnectionPolicy policy;
        policy.gracefulCloseTimeout = std::chrono::milliseconds{10};
        FakeStream stream;
        stream.blockWhenEmpty(true);
        Connection connection{std::move(stream), policy};
        connection.closeAfterWrite();
        assert(connection.onWritable().has_value());
        const auto closeDeadline = connection.deadline();
        assert(closeDeadline.has_value());
        assert(connection.expiredTimeout(*closeDeadline) == Timeout::GracefulClose);
        assert(connection.onTimeout(*closeDeadline).value());
        assert(connection.closed());
    }
}
