#include <algorithm>
#include <cassert>
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

        private:
            static constexpr std::size_t MaxWriteSize = 5;

            std::string input_;
            std::string output_;
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
        assert(connection.closed());
        assert(connection.interests() == Interests{});
        assert(!connection.queueOutput("ignored"));
    }
}
