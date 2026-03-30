#include "../include/ninttp/socket/client/client_socket.hpp"
#include "../include/ninttp/socket/server/server_socket.hpp"

int main(){
    ninttp::TcpV4ListenerSocket ls;
    ls.bind(ninttp::v4Addr_loopback, 9999);
    ls.listen(1);

    ninttp::TcpV4ClientSocket cs;
    cs.connect(ninttp::v4Addr_loopback, 9999);
    
    auto ss = ls.acceptOne();

    //write and read
}