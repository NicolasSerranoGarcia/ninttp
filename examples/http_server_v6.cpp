#include <iostream>

#include "../include/ninttp/http/http_server.hpp"

using namespace ninttp;

void serverV6(){
	httpServer<http_1_0, IPv6Endpoint> serverV6;
    serverV6.doGET("/", [](Response& response){ response.body = std::string("Hello, World!"); });
    serverV6.listen(IPv6Endpoint::loopback(8080));
}

int main(){
    serverV6();
}
