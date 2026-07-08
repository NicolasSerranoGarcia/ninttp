#include <iostream>

#include "../include/ninttp/http/http_server.hpp"

using namespace ninttp;

int server(){
    try{
        httpServer server;
        server.doGET("/", [](Response& response){ response.body = std::string("Hello, World!"); });
        if(auto listened = server.listen(IPv4Endpoint::loopback(8080)); !listened.has_value()){
            std::cerr << listened.error().what << std::endl;
            return 1;
        }
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
    return server();
}
