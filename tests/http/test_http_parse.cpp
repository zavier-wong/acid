//
// Created by zavier on 2021/12/11.
//
#include <spdlog/spdlog.h>
#include <acid/net/socket.h>
#include "acid/http/parse.h"
using namespace acid;

void test_response_parse(){
    acid::http::HttpResponseParser::ptr parser(new acid::http::HttpResponseParser());
    char res[100] = "HTTP/1.0 404 NOT FOUND\r\nContent-Length: 23330\r\nDate: Thu,08 Mar 202107:17:51 GMT\r\n\r\n";
    parser->execute(res, strlen(res));
    SPDLOG_INFO(parser->getData()->toString());
}
const char test_request_data[] = "POST / HTTP/1.1\r\n"
                                 "Host: www.acid.icu\r\n"
                                 "Content-Length: 10\r\n\r\n";

void test_request() {
    acid::http::HttpRequestParser parser;
    std::string tmp = test_request_data;
    size_t s = parser.execute(&tmp[0], tmp.size());
    SPDLOG_INFO("execute rt={} has_error={} is_finished={} total={} content_length={}",
                s, parser.hasError(), parser.isFinished(), tmp.size(), parser.getContentLength());
    tmp.resize(tmp.size() - s);
    SPDLOG_INFO(parser.getData()->toString());
    SPDLOG_INFO(tmp);
}

const char test_response_data[] = "HTTP/1.1 200 OK\r\n"
                                  "Date: Tue, 04 Jun 2050 15:43:56 GMT\r\n"
                                  "Server: Apache\r\n"
                                  "Last-Modified: Tue, 12 Jan 2010 13:48:00 GMT\r\n"
                                  "ETag: \"51-47cf7e6ee8400\"\r\n"
                                  "Accept-Ranges: bytes\r\n"
                                  "Content-Length: 81\r\n"
                                  "Cache-Control: max-age=86400\r\n"
                                  "Expires: Wed, 05 Jun 2019 15:43:56 GMT\r\n"
                                  "Connection: Close\r\n"
                                  "Content-Type: text/html\r\n\r\n"
                                  "<html>\r\n"
                                  "<meta http-equiv=\"refresh\" content=\"0;url=http://www.baidu.com/\">\r\n"
                                  "</html>\r\n";

void test_response() {
    acid::http::HttpResponseParser parser;
    std::string tmp = test_response_data;
    size_t s = parser.execute(&tmp[0], tmp.size());
    SPDLOG_INFO("execute rt={} has_error={} is_finished={} total={} content_length={} tmp[s]={}",
                s, parser.hasError(), parser.isFinished(), tmp.size(), parser.getContentLength(), tmp[s]);

    tmp.resize(tmp.size() - s);

    SPDLOG_INFO(parser.getData()->toString());
    SPDLOG_INFO(tmp);
}
int main(){
    test_request();
    test_response();
}