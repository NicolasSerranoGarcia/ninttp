#include <iostream>
#include <stdexcept>

#include "../include/ninttp/http/http_client.hpp"

using namespace ninttp;

int client(){
    try{
        httpClient client(IPv4Endpoint::loopback(8080), "localhost");

        std::expected<Response, NinError> got;

        if(got = client.GET("/"); !got.has_value()){
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
    return client();
}
