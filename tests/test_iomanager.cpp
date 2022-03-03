//
// Created by zavier on 2021/11/30.
//
#include "acid/acid.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <iostream>
#include <sys/epoll.h>
#include "stdio.h"
static acid::Logger::ptr  g_logger = ACID_LOG_ROOT();


int setnonblocking( int fd )   //自定义函数，用于设置套接字为非阻塞套接字
{
    int old_option = fcntl( fd, F_GETFL );
    int new_option = old_option | O_NONBLOCK;
    fcntl( fd, F_SETFL, new_option );        //设置为非阻塞套接字
    return old_option;
}

void test_fiber(){
    ACID_LOG_DEBUG(g_logger) << "test_fiber start";
    acid::Fiber::YieldToReady();
    sleep(3);
    ACID_LOG_DEBUG(g_logger) << "test_fiber end";
}

void test1(){
    acid::IOManager ioManager(2);
    ioManager.submit(&test_fiber);
    ioManager.submit(std::make_shared<acid::Fiber>(test_fiber));
}
void test2(){
    ACID_LOG_DEBUG(g_logger) << acid::Fiber::GetThis()->getState();
    acid::IOManager ioManager(2);
    //int sock = socket(AF_INET, SOCK_STREAM, 0);
    //setnonblocking(sock);

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr.s_addr);
//    ioManager.addEvent(sock,acid::IOManager::READ,[sock](){
//        while(1){
//            char buff[100];
//            memset(buff,0,100);
//            recv(sock,buff,100,0);
//            //puts(buff);
//            ACID_LOG_INFO(g_logger) << "Server: " <<buff;
//            acid::Fiber::YieldToReady();
//        }
//    });
    acid::IOManager::GetThis()->addEvent(STDIN_FILENO, acid::IOManager::READ, [](){
        while (true){
            ACID_LOG_DEBUG(g_logger) << "connected r";
            char buff[100];
            ACID_LOG_INFO(g_logger) << "STDIN: " << fgets(buff,100,stdin);
            acid::Fiber::YieldToReady();
            //break;

        }
    });
    //acid::IOManager::GetThis()->cancelEvent(STDIN_FILENO,acid::IOManager::READ);
    //int cfd = connect(sock, (const sockaddr*)&addr, sizeof(addr));
    //ACID_LOG_INFO(g_logger)<<cfd;
    //setnonblocking(cfd);


}

void test3(){
    ACID_LOG_DEBUG(g_logger) << acid::Fiber::GetThis()->getState();
    acid::IOManager ioManager(3);
    acid::IOManager::GetThis()->addEvent(STDIN_FILENO, acid::IOManager::READ, [](){
        while (true){
            ACID_LOG_DEBUG(g_logger) << "connected r";
            char buff[100];
            ACID_LOG_INFO(g_logger) << fgets(buff,100,stdin);
            //acid::Fiber::YieldToReady();
            //break;
            //acid::IOManager::GetThis()->addEvent(STDIN_FILENO, acid::IOManager::READ,)
        }
    });
}
void test_cb(){
    ACID_LOG_INFO(g_logger) << "test cb";
    char buff[100];
    ACID_LOG_INFO(g_logger) << fgets(buff,100,stdin);
    acid::IOManager::GetThis()->addEvent(STDIN_FILENO, acid::IOManager::READ, &test_cb);
}
void test4(){
    ACID_LOG_DEBUG(g_logger) << acid::Fiber::GetThis()->getState();
    acid::IOManager ioManager(2);
    //acid::IOManager::GetThis()->addEvent(STDIN_FILENO, acid::IOManager::READ, &test_cb);
}

int main(){
    //acid::Fiber::EnableFiber();
    test1();
    //std::cout<<sizeof(D);

}