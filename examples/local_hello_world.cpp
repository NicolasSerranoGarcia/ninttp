#include <unistd.h>

#include <array>
#include <cstring>
#include <exception>
#include <iostream>

#include "../include/ninttp/socket/endpoints.hpp"
#include "../include/ninttp/socket/socket.hpp"

int main(){
	try {
		using Endpoint = ninttp::Ipv4Endpoint;
		using ClientSocket = ninttp::StreamSocket<Endpoint>;
		using ServerSocket = ninttp::ListenerSocket<Endpoint, ClientSocket>;

		constexpr auto message = "hello world";
		const auto port = static_cast<uint16_t>(40000 + (::getpid() % 10000));
		const auto endpoint = ninttp::Ipv4Endpoint::loopback(port);

		ServerSocket server(ninttp::Domain::IPv4, ninttp::Protocol::Tcp);

		if (auto bindResult = server.bind(endpoint); !bindResult.has_value()) {
			std::cerr << "bind failed: " << bindResult.error().msg() << '\n';
			return 1;
		}

		if (auto listenResult = server.listen(1); !listenResult.has_value()) {
			std::cerr << "listen failed: " << listenResult.error().msg() << '\n';
			return 1;
		}

		ClientSocket client(ninttp::Domain::IPv4, ninttp::Protocol::Tcp);
		auto connectResult = client.connect(endpoint);
		if (!connectResult.has_value()) {
			std::cerr << "connect failed: " << connectResult.error().msg() << '\n';
			return 1;
		}

		auto sendResult = client.send(message, std::strlen(message));
		if (!sendResult.has_value()) {
			std::cerr << "send failed: " << sendResult.error().msg() << '\n';
			return 1;
		}

		std::cout << "client sent: " << message << '\n';

		auto acceptResult = server.accept();
		if (!acceptResult.has_value()) {
			std::cerr << "accept failed: " << acceptResult.error().msg() << '\n';
			return 1;
		}

		auto connection = std::move(acceptResult.value());
		std::array<char, 64> buffer{};
		auto receiveResult = connection.receive(buffer.data(), buffer.size() - 1);
		if (!receiveResult.has_value()) {
			std::cerr << "receive failed: " << receiveResult.error().msg() << '\n';
			return 1;
		}

		std::cout << "server received: " << buffer.data() << '\n';
		return 0;
	} catch (const ninttp::socketError& error) {
		std::cerr << "socket setup failed: " << error.msg() << '\n';
		return 1;
	} catch (const std::exception& error) {
		std::cerr << "example failed: " << error.what() << '\n';
		return 1;
	}
}