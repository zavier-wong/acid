//
// Created by zavier on 2021/12/12.
//

#include "acid/http/http.h"
#include "acid/log.h"
static acid::Logger::ptr g_logger = ACID_LOG_ROOT();
void test_request(){
    acid::http::HttpRequest h;
    //h.setPath("/index.h");
    h.setHeader("host","vvip.icu");
    h.setBody("Hello, World");

    ACID_LOG_INFO(g_logger) << '\n' << h.toString();
}
void test_response(){
    acid::http::HttpResponse r;
    r.setHeader("host","vvip.icu");
    r.setBody("Hello, World");
    r.setStatus(404);
    ACID_LOG_INFO(g_logger) << '\n' << r.toString();
}
int main(){

    test_response();
}