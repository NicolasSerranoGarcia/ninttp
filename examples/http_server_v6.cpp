#include <iostream>

#include "../include/ninttp/http/http_server.hpp"

using namespace ninttp;

int serverV6(){
    try{
        httpServer<http_1_0, IPv6Endpoint> serverV6;
        serverV6.doGET("/", [](Response& response){ response.body = std::string("Hello, World!"); });
        if(auto listened = serverV6.listen(IPv6Endpoint::loopback(8080)); !listened.has_value()){
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
    return serverV6();
}
