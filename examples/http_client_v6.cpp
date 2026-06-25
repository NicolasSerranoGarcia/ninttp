#include <iostream>

#include "../include/ninttp/http/http_client.hpp"

void clientV6  (){

    try{
        ninttp::httpClient<ninttp::IPv6Endpoint> clientV6(ninttp::IPv6Endpoint::loopback(8080));

        std::expected<ninttp::Response, ninttp::SocketError> got;

        if(got = clientV6  .GET("/"); !got.has_value())
            std::cerr << got.error().msg() << std::endl;

        std::cout << got.value() << std::endl;

    } catch(ninttp::SocketError& err){
        std::cerr << err.msg();
    } catch(std::bad_alloc& allocErr){
        std::cerr << allocErr.what();
    }
}

int main(){
    clientV6   ();
}
