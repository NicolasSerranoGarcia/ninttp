#include <iostream>

#include "../include/ninttp/http/http_server.hpp"

using namespace ninttp;

int serverV6(){
    try{
        httpServer<http_1_0, IPv6Endpoint> serverV6;
        if(!serverV6.registerHost("localhost") ||
           !serverV6.registerHandler("localhost", "/", "GET", [](const Request&, Response& response){ response.setContent("Hello, World!"); })){
            std::cerr << "failed to register HTTP route" << std::endl;
            return 1;
        }
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
