//
// Created by zavier on 2021/12/20.
//

#include "acid/log.h"
#include "acid/net/uri.h"
using namespace acid;
static Logger::ptr g_logger = ACID_LOG_ROOT();


int main(){
    std::string http = "www.vvip.icu:8080/blog/";
    std::string https = "https://baidu.com/add?a=1&b=2#aaa";
    std::string ftp = "ftp://admin:passwd@www.myftp.com/profile";
    std::string file = "file:///c:/desktop/a.cpp";
    std::string magnet = "magnet:?xt=urn:btih:2F9D75F4CB3385CB06FFB30695A34FAC7033902C";
    ACID_LOG_INFO(g_logger) << Uri::Create(http)->toString();
    ACID_LOG_INFO(g_logger) << Uri::Create(https)->toString();
    ACID_LOG_INFO(g_logger) << Uri::Create(ftp)->toString();
    ACID_LOG_INFO(g_logger) << Uri::Create(file)->toString();
    ACID_LOG_INFO(g_logger) << Uri::Create(magnet)->toString();
    ACID_LOG_INFO(g_logger) << Uri::Create(https)->createAddress()->toString();
}