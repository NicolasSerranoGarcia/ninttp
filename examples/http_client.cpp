#include <iostream>

#include "../include/ninttp/http/http_client.hpp"

void client(){
    ninttp::httpClient client(ninttp::IPv4Endpoint::loopback(8080));

    if(const auto got = client.GET("/"); !got.has_value())
        std::cout << got.error().msg() << std::endl;

    std::cout << client.GET("/").value() << std::endl;
}

int main(){
    client();
}
