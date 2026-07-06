#include <iostream>

#include "../include/ninttp/http/http_server.hpp"

using namespace ninttp;

void server(){
	httpServer server;
    server.doGET("/hello", [](Response& response){ response.body = std::string("Hello, World!"); });
    server.listen(IPv4Endpoint::loopback(8080));
}

int main(){
    server();
}
