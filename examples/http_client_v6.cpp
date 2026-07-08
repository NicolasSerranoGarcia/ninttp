#include <iostream>
#include <stdexcept>

#include "../include/ninttp/http/http_client.hpp"

using namespace ninttp;

int clientV6(){
    try{
        httpClient<http_1_0, IPv6Endpoint> clientV6(IPv6Endpoint::loopback(8080), "localhost");

        std::expected<Response, NinError> got;

        if(got = clientV6.GET("/"); !got.has_value()){
            std::cerr << got.error().what << std::endl;
            return 1;
        }

        std::cout << got.value() << std::endl;

    } catch(NinError& err){
        std::cerr << err.what << std::endl;
        return 1;
    } catch(const std::exception& err){
        std::cerr << err.what() << std::endl;
        return 1;
    }

    return 0;
}

int main(){
    return clientV6();
}
