#include <iostream>

#include "../include/ninttp/http/http_client.hpp"

void client(){
    ninttp::httpClient client(ninttp::IPv4Endpoint::loopback(8080));

    std::expected<ninttp::Response, ninttp::SocketError> got;

    if(got = client.GET("/"); !got.has_value()){
        try{
            std::cerr << got.error().msg() << std::endl;
        } catch(const std::bad_alloc& err){
            std::cerr << err.what() << std::endl;
        }
    }

    std::cout << got.value() << std::endl;
}

int main(){
    client();
}
