#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <iostream>

#include "../include/ninttp/socket/endpoints.hpp"
#include "../include/ninttp/socket/socket.hpp"

int main() {
	ninttp::internal::SelectedBackend::AddressStorageT storage{};
	auto* ipv4 = reinterpret_cast<sockaddr_in*>(&storage);
	ipv4->sin_family = AF_INET;
	ipv4->sin_port = htons(8080);
	ipv4->sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	auto endpoint = ninttp::Ipv4Endpoint::fromStorage(storage);
	auto roundtripStorage = endpoint.toStorage();
	auto roundtripLen = endpoint.storageLen();

	std::cout << "endpoint roundtrip len: " << static_cast<unsigned>(roundtripLen) << '\n';

	ninttp::StreamSocket<ninttp::Ipv4Endpoint> client(ninttp::Domain::IPv4, ninttp::Protocol::Tcp);
	auto connectResult = client.connect(endpoint);
	if (!connectResult.has_value()) {
		std::cout << "connect failed (expected in demo without server): " << connectResult.error().context() << '\n';
	} else {
		std::cout << "connect succeeded" << '\n';
	}

	(void)roundtripStorage;
	return 0;
}