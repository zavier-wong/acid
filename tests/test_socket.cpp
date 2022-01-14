//
// Created by zavier on 2021/12/6.
//
#include "acid/log.h"
#include "acid/io_manager.h"
#include "acid/net/socket.h"
#include "acid/net/socket_stream.h"
#include "acid/byte_array.h"
static acid::Logger::ptr g_logger = ACID_LOG_ROOT();
void test_socket(){
    acid::Address::ptr addr = acid::IPAddress::Create("vvip.icu",80);
    if(addr){
        ACID_LOG_DEBUG(g_logger) << addr->toString();
    } else {
        return;
    }
    acid::Socket::ptr sock = acid::Socket::CreateTCP(addr);
    if(sock->connect(addr)){
        ACID_LOG_DEBUG(g_logger) << addr->toString() << " connected";
    } else {
        return;
    }
    char buff[]="GET / HTTP/1.0\r\n\r\n";
    sock->send(buff,sizeof buff);
    std::string str;
    str.resize(4096);
    sock->recv(&str[0],str.size());
    ACID_LOG_INFO(g_logger)<<str;
}
std::set<acid::Socket::ptr> clients;
void sendAll(acid::Socket::ptr sock, const char *msg, int len){
    char* buff = new char[4096];
    sprintf(buff,"%d says: ",sock->getSocket());
    strcat(buff,msg);
    puts(buff);
    for(auto i : clients){
        if(i != sock){
            i->send(buff, strlen(buff));
        }
    }
    delete[] buff;
}
void test_server(){
    acid::Address::ptr address = acid::IPv4Address::Create("127.0.0.1", 8080);
    acid::Socket::ptr sock = acid::Socket::CreateTCPSocket();
    sock->bind(address);
    sock->listen();
    while(1){
        acid::Socket::ptr client = sock->accept();
        clients.insert(client);
        sendAll(client,"New User, welcome him\n", 21);
        acid::IOManager::GetThis()->submit([client]{
            char *buff = new char[4096];

            while(1){
                int n = client->recv(buff,4096);
                if(n == 0){
                    ACID_LOG_INFO(g_logger) << "user quit";
                    break;
                }
                buff[n] = 0;
                //send(fd,buff,n,0);
                sendAll(client,buff,n);
            }
            sendAll(client,"user quit\n", 10);
            delete[] buff;
            clients.erase(client);
            client->close();
        });
    }
}
void test_byte_array() {
    acid::Address::ptr addr = acid::IPAddress::Create("localhost",8080);
    if(addr){
        ACID_LOG_DEBUG(g_logger) << addr->toString();
    } else {
        return;
    }
    acid::Socket::ptr sock = acid::Socket::CreateTCP(addr);
    if(sock->connect(addr)){
        ACID_LOG_DEBUG(g_logger) << addr->toString() << " connected";
    } else {
        return;
    }

    char buff[]="hello";
    acid::ByteArray::ptr bt(new acid::ByteArray());
    bt->write(buff, 6);
    bt->setPosition(0);
    //sock->send(buff,sizeof buff);

    acid::SocketStream ss(sock);
    ss.writeFixSize(bt, 6);
    return;
    std::string str;
    str.resize(4096);
    sock->recv(&str[0],str.size());
    ACID_LOG_INFO(g_logger)<<str;
}
int main(){
    acid::IOManager ioManager{};
    ioManager.submit(&test_byte_array);
    //test_socket();
}