#include <iostream>

#include "../include/ninttp/http/http_client.hpp"

using namespace ninttp;

void client(){

    try{
        httpClient client(IPv4Endpoint::loopback(8080));

        std::expected<Response, NinError> got;

        if(got = client.GET("/"); !got.has_value())
            std::cerr << got.error().what << std::endl;

        std::cout << got.value() << std::endl;

    } catch(SocketError& err){
        std::cerr << err.what();
    }
}

int main(){
    client();
}
