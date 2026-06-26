#include <iostream>

#include "../include/ninttp/http/http_client.hpp"

using namespace ninttp;

void client(){

    try{
        httpClient client(IPv4Endpoint::loopback(8080));

        std::expected<Response, SocketError> got;

        if(got = client.GET("/"); !got.has_value())
            std::cerr << got.error().msg() << std::endl;

        std::cout << got.value() << std::endl;

    } catch(SocketError& err){
        std::cerr << err.msg();
    } catch(std::bad_alloc& allocErr){
        std::cerr << allocErr.what();
    }
}

int main(){
    client();
}
