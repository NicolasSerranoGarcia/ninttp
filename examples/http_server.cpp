#include <iostream>

#include "../include/ninttp/http/http_server.hpp"

void server(){
	ninttp::httpServer server;
    server.doGET("/", [](ninttp::Response& response){ response.body = std::string("Hello, World!"); });
    server.listen(ninttp::IPv4Endpoint::loopback(8080));
}

int main(){
    server();
}
