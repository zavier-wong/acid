//
// Created by zavier on 2021/12/11.
//

#include "acid/acid.h"
using namespace acid;
static acid::Logger::ptr g_logger = ACID_LOG_ROOT();


void test_response_parse(){
    acid::http::HttpResponseParser::ptr parser(new acid::http::HttpResponseParser());
    char res[100] = "HTTP/1.0 404 NOT FOUND\r\nContent-Length: 23330\r\nDate: Thu,08 Mar 202107:17:51 GMT\r\n\r\n";
    parser->execute(res, strlen(res));
    ACID_LOG_INFO(g_logger) << "\n" << parser->getData()->toString();

}
const char test_request_data[] = "POST / HTTP/1.1\r\n"
                                 "Host: www.acid.icu\r\n"
                                 "Content-Length: 10\r\n\r\n";

void test_request() {
    acid::http::HttpRequestParser parser;
    std::string tmp = test_request_data;
    size_t s = parser.execute(&tmp[0], tmp.size());
    ACID_LOG_ERROR(g_logger) << "execute rt=" << s
                              << "has_error=" << parser.hasError()
                              << " is_finished=" << parser.isFinished()
                              << " total=" << tmp.size()
                              << " content_length=" << parser.getContentLength();
    tmp.resize(tmp.size() - s);
    ACID_LOG_INFO(g_logger) << parser.getData()->toString();
    ACID_LOG_INFO(g_logger) << tmp;
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
    ACID_LOG_ERROR(g_logger) << "execute rt=" << s
                              << " has_error=" << parser.hasError()
                              << " is_finished=" << parser.isFinished()
                              << " total=" << tmp.size()
                              << " content_length=" << parser.getContentLength()
                              << " tmp[s]=" << tmp[s];

    tmp.resize(tmp.size() - s);

    ACID_LOG_INFO(g_logger) << parser.getData()->toString() << tmp;
    //ACID_LOG_INFO(g_logger) << tmp;
}
void test_vvip(){
    std::string str;
    str.resize(4096);
    acid::http::HttpRequest request;
    request.setHeader("Host", "vvip.icu");
    auto sock = acid::Socket::CreateTCPSocket();
    sock->connect(acid::IPAddress::Create("vvip.icu",80));
    acid::ByteArray buff;
    buff.writeStringWithoutLength(request.toString());
    //sock->send()
    buff.setPosition(0);
    //sock->send(buff);

    buff.clear();
    acid::ByteArray buffer;
    //int n = sock->recv(buffer, 8000);
    int n = sock->recv(&str[0], 4096);
    ACID_LOG_INFO(g_logger) << n;
    ACID_LOG_INFO(g_logger) << str;
    n = sock->recv(&str[0], 4096);
    ACID_LOG_INFO(g_logger) << n;
    ACID_LOG_INFO(g_logger) << str;
    //str = buff.toString();
    //ACID_LOG_INFO(g_logger) << buffer.toString();
//    acid::http::HttpResponseParser response;
//    response.execute(&str[0],4096);
//    ACID_LOG_INFO(g_logger) << response.getData()->toString() ;
//    ACID_LOG_INFO(g_logger) << response.getContentLength();
//    ACID_LOG_INFO(g_logger) << str.substr(0,response.getContentLength());
}
int main(){
    test_response();
    //IOManager ioManager{1};
    //ioManager.submit(&test_response);
    //ioManager.submit(&test_response_parse);
}