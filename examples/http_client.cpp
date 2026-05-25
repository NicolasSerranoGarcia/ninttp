#include <iostream>

#include "../include/ninttp/http/http.hpp"

void client(){
    ninttp::httpClient client(ninttp::IPv4Endpoint::loopback(8080));
    std::cout << client.GET("/").value() << std::endl;
}

int main(){
    client();
}