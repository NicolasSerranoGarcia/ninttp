#include <iostream>

#include "../include/ninttp/http/http.hpp"

void client(){
    ninttp::httpClient client(ninttp::IPv4Endpoint::loopback(8080));
    client.GET("/");
}

int main(){
    client();
}