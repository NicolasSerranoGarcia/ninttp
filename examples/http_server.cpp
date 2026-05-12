#include <iostream>

#include <thread>
using namespace std::literals;

#include "../include/ninttp/http/http.hpp"

void server(){
	ninttp::httpServer server;
    server.listen(ninttp::IPv4Endpoint::loopback(8080));
}

int main(){
    server();
}