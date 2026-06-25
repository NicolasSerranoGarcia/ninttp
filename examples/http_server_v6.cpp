#include <iostream>

#include "../include/ninttp/http/http_server.hpp"

void serverV6(){
	ninttp::httpServer<ninttp::IPv6Endpoint> serverV6;
    serverV6.doGET("/", [](ninttp::Response& response){ response.body = std::string("Hello, World!"); });
    serverV6.listen(ninttp::IPv6Endpoint::loopback(8080));
}

int main(){
    serverV6();
}
