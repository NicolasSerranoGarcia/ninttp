#include "../include/ninttp/ninttp.hpp"

#include <thread>
#include <chrono>

using namespace ninttp;

const char* helloWord = "Hello, World!";
constexpr uint16_t demoPort = 8080;

void server(){
    try{
        ListenerSocket<Ipv4Endpoint, StreamSocket<Ipv4Endpoint>> listener(Domain::IPv4, Protocol::Tcp);

        listener.bind(Ipv4Endpoint::fromOctets(0, 0, 0, 0, demoPort));

        listener.listen(1);
        
        auto server = listener.accept();
        
        server.send(helloWord, std::strlen(helloWord));

    } catch(socketError& e){
        std::cerr << e.msg() << std::endl;
    }
}

void client(){
    char buff[1024]{};

    try{
        StreamSocket<Ipv4Endpoint> cli(Domain::IPv4, Protocol::Tcp);
        
        cli.connect(Ipv4Endpoint::fromOctets(127, 0, 0, 1, demoPort));

        
        cli.receive(buff, 1024 * sizeof(char));

    } catch(socketError& e){
        std::cerr << e.msg() << std::endl;
        return;
    }

    std::string msg(buff);
    std::cout << msg << std::endl;
}

int main(){
    std::thread tServer(server);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    std::thread tclient(client);

    tServer.join();
    tclient.join();

    return EXIT_SUCCESS;
}