//
// Created by zavier on 2021/12/12.
//
#include <spdlog/spdlog.h>
#include "acid/http/http.h"

void test_request(){
    acid::http::HttpRequest h;
    //h.setPath("/index.h");
    h.setHeader("host","vvip.icu");
    h.setBody("Hello, World");
    SPDLOG_INFO(h.toString());
}
void test_response(){
    acid::http::HttpResponse r;
    r.setHeader("host","vvip.icu");
    r.setBody("Hello, World");
    r.setStatus(404);
    SPDLOG_INFO(r.toString());
}
int main(){
    test_response;
}