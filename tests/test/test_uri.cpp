//
// Created by zavier on 2021/12/20.
//

#include "acid/net/uri.h"
using namespace acid;

int main(){
    std::string http = "www.baidu.com/s?wd=%E4%BD%A0%E5%A5%BD";
    std::string https = "https://baidu.com/add?a=1&b=2#aaa";
    std::string ftp = "ftp://admin:passwd@www.myftp.com/profile";
    std::string file = "file:///c:/desktop/a.cpp";
    std::string magnet = "magnet:?xt=urn:btih:2F9D75F4CB3385CB06FFB30695A34FAC7033902C";
    SPDLOG_INFO(Uri::Create(http)->toString());
    SPDLOG_INFO(Uri::Create(https)->toString());
    SPDLOG_INFO(Uri::Create(ftp)->toString());
    SPDLOG_INFO(Uri::Create(file)->toString());
    SPDLOG_INFO(Uri::Create(magnet)->toString());
    SPDLOG_INFO(Uri::Create(https)->createAddress()->toString());
}