//
// Created by zavier on 2021/12/8.
//

#include <stdint.h>
#include <iostream>
#include "stdio.h"
#include <bit>
#include "acid/net/address.h"
#include "acid/common/config.h"
char buff[100000];
void test_http(){
    acid::Address::ptr address = acid::IPAddress::Create("baidu.com", 80);
    //address->insert(std::cout);
    spdlog::info(address->toString());
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    int rt = connect(fd, address->getAddr(), address->getAddrLen());
    spdlog::info(rt);
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
        spdlog::info(i->toString());
    }
}
void test_iface(){
    std::multimap<std::string,std::pair<acid::Address::ptr, uint32_t>> r;
    acid::Address::GetInterfaceAddresses(r,AF_INET6);
    for(auto item:r){
        spdlog::info("{}, {}, {}", item.first, item.second.first->toString(), item.second.second);
    }
}
void test_ipv4(){
    auto addr = acid::IPAddress::Create("iptv.tsinghua.edu.cn");
    //auto addr = acid::IPAddress::Create("127.0.0.8");
    if(addr) {
        spdlog::info(addr->toString());
    }
}
int main(){
    test_http();
    spdlog::warn("====================");
    test_addr();
    spdlog::warn("====================");
    test_iface();
    spdlog::warn("====================");
    test_ipv4();
}