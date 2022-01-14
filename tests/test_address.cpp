//
// Created by zavier on 2021/12/8.
//

#include <stdint.h>
#include <iostream>
#include "stdio.h"
#include <bit>
#include "acid/net/address.h"
#include "acid/log.h"
#include "acid/io_manager.h"
#include "acid/config.h"
static acid::Logger::ptr g_logger = ACID_LOG_ROOT();
char buff[100000];
void test_http(){
    acid::Address::ptr address = acid::IPAddress::Create("baidu.com", 80);
    //address->insert(std::cout);
    ACID_LOG_INFO(g_logger) << address->toString();
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    int rt = connect(fd, address->getAddr(), address->getAddrLen());
    ACID_LOG_INFO(g_logger) << rt;
    //read(fd,buff,100);
    const char data[] = "GET / HTTP/1.0\r\n\r\n";
    rt = send(fd, data, sizeof(data), 0);
    char *p = buff;
    while((rt = recv(fd,p,4096,0)) > 0){
        p += rt;
    }

    puts(buff);
}
void test_addr(){
    std::vector<acid::Address::ptr> res;
    acid::Address::Lookup(res,"iptv.tsinghua.edu.cn");
    for(auto i:res){
        std::cout<<i->toString()<<std::endl;
    }
}
void test_iface(){
    std::multimap<std::string,std::pair<acid::Address::ptr, uint32_t>> r;
    acid::Address::GetInterfaceAddresses(r,AF_INET6);
    for(auto item:r){
        std::cout<<item.first<<','<<item.second.first->toString()<<','<<item.second.second<<'\n';
    }
}
void test_ipv4(){
    auto addr = acid::IPAddress::Create("iptv.tsinghua.edu.cn");
    //auto addr = acid::IPAddress::Create("127.0.0.8");
    if(addr) {
        ACID_LOG_INFO(g_logger) << addr->toString();
    }
}
int main(){
    ACID_LOG_INFO(g_logger) << "Main";
    //acid::IOManager ioManager{};
    //ioManager.submit(&test_http);

    //test_addr();
    //test_iface();
    test_ipv4();
}