#include <iostream>

#include "../include/ninttp/http/http_client.hpp"

using namespace ninttp;

void clientV6(){

    try{
        httpClient<http_1_0, IPv6Endpoint> clientV6(IPv6Endpoint::loopback(8080));

        std::expected<Response, internal::httpParseError> got;

        if(got = clientV6.GET("/"); !got.has_value())
            std::cerr << got.error().what << std::endl;

        std::cout << got.value() << std::endl;

    } catch(SocketError& err){
        std::cerr << err.msg();
    } catch(std::bad_alloc& allocErr){
        std::cerr << allocErr.what();
    }
}

int main(){
    clientV6();
}
