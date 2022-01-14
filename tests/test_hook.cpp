//
// Created by zavier on 2021/12/4.
//

#include "acid/acid.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
static auto g_logger = ACID_LOG_ROOT();

void test_sleep(){
    acid::IOManager ioManager(1);
    ioManager.submit([](){
        ACID_LOG_INFO(g_logger) << "2 sec start";
        usleep(2000'000);
        ACID_LOG_INFO(g_logger) << "2 sec end";
    });
    ioManager.submit([](){
        ACID_LOG_INFO(g_logger) << "3 sec start";
        sleep(3);
        ACID_LOG_INFO(g_logger) << "3 sec end";
    });
    ACID_LOG_INFO(g_logger) << "test sleep";
}
std::set<int> fds;
void sendAll(int fd, const char *msg, int len){
    char* buff = new char[4096];
    sprintf(buff,"%d says: ",fd);
    strcat(buff,msg);
    puts(buff);
    for(auto i : fds){
        if(i != fd){
            send(i,buff,strlen(buff),0);
        }
    }
    delete[] buff;
}
void test_connect(){
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8081);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr.s_addr);

    bind(sock, reinterpret_cast<const sockaddr *>(&addr), sizeof(addr));

    listen(sock,5);
    ACID_LOG_INFO(g_logger) << "begin accept";
    int len = sizeof(addr);
    int fd = 0;
    while(( fd = accept(sock, (sockaddr*)&addr, reinterpret_cast<socklen_t *>(&len)))){
        fds.insert(fd);
        ACID_LOG_INFO(g_logger) << "accept rt=" << fd << " errno=" << errno;
        sendAll(fd,"New User, welcome him\n", 21);
        acid::IOManager::GetThis()->submit([fd]{
            char *buff = new char[4096];
            while(1){
                int n = recv(fd, buff, 4096, 0);
                if(n == 0){
                    ACID_LOG_INFO(g_logger) << "user quit";
                    break;
                }
                buff[n] = 0;

                //send(fd,buff,n,0);
                sendAll(fd,buff,n);
            }
            sendAll(fd,"user quit\n", 10);
            delete[] buff;
            fds.erase(fd);
            close(fd);
        });
        ACID_LOG_INFO(g_logger) << "next";
    }
}

void test_echo(){
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr.s_addr);

    bind(sock, reinterpret_cast<const sockaddr *>(&addr), sizeof(addr));

    listen(sock,5);
    ACID_LOG_INFO(g_logger) << "begin accept";
    int len = sizeof(addr);
    int rt = accept(sock, (sockaddr*)&addr, reinterpret_cast<socklen_t *>(&len));
    ACID_LOG_INFO(g_logger) << "accept rt=" << rt << " errno=" << errno;


    char buff[4096]{0};
    while(1){
        int n = recv(rt, buff, 4096, 0);
        if(n == 0){
            ACID_LOG_INFO(g_logger) << "user quit";
            return;
        }
        buff[n] = 0;
        puts(buff);
        send(rt,buff,n,0);
    }

}


void test_http_client(){
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(80);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr.s_addr);

    ACID_LOG_INFO(g_logger) << "begin connect";
    int rt = connect(sock, (const sockaddr*)&addr, sizeof(addr));
    ACID_LOG_INFO(g_logger) << "connect rt=" << rt << " errno=" << errno;

    if(rt) {
        return;
    }

    const char data[] = "GET / HTTP/1.0\r\n\r\n";
    rt = send(sock, data, sizeof(data), 0);
    ACID_LOG_INFO(g_logger) << "send rt=" << rt << " errno=" << errno;

    if(rt <= 0) {
        return;
    }

    std::string buff;
    buff.resize(4096);

    rt = recv(sock, &buff[0], buff.size(), 0);
    ACID_LOG_INFO(g_logger) << "recv rt=" << rt << " errno=" << errno;

    if(rt <= 0) {
        return;
    }

    buff.resize(rt);
    ACID_LOG_INFO(g_logger) << buff;
}
void setResponse(char *buff,int n)
{
    bzero(buff, n);
    strcat(buff, "HTTP/1.1 200 OK\r\n");
    strcat(buff, "Connection: close\r\n");
    strcat(buff, "\r\n");
    strcat(buff, "Hello\n");
}
void read_request_headrs(int fd){
    char line[4096]={};
    int idx=0;
    while ((idx=recv(fd,line,4096,0))>0){
        line[idx]=0;
        //printf("%s",line);
        if(strncmp(line,"\r\n\r\n",2)==0)
            break;
    }
}
void test_http_server(){
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8081);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr.s_addr);

    bind(sock, reinterpret_cast<const sockaddr *>(&addr), sizeof(addr));

    listen(sock,5);
    ACID_LOG_INFO(g_logger) << "begin accept";
    int len = sizeof(addr);
    int fd = 0;
    while(( fd = accept(sock, (sockaddr*)&addr, reinterpret_cast<socklen_t *>(&len)))){
        //ACID_LOG_INFO(g_logger) << "accept rt=" << fd << " errno=" << errno;
        acid::IOManager::GetThis()->submit([fd]{
            char *buff = new char[4096];
            //read_request_headrs(fd);
            setResponse(buff,4096);
            send(fd,buff,strlen(buff),0);
            close(fd);
        });
        //ACID_LOG_INFO(g_logger) << "next";
    }
}
int main(){
    acid::Config::LoadFromFile("config/log.yaml");
    acid::IOManager ioManager{};
    //ioManager.submit(&test_http);
    //test_sock();
    //test_echo();
    ioManager.submit(&test_http_server);
}