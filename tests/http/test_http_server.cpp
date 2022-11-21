//
// Created by zavier on 2021/12/15.
//
#include <sstream>
#include "acid/http/http_server.h"
using namespace acid;
int main(){
    // GetLogInstance()->set_level(spdlog::level::off);

    Address::ptr address = Address::LookupAny("0.0.0.0:8081");
    http::HttpServer server(true);
    // server.getServletDispatch()->addGlobServlet("/*", std::make_shared<http::FileServlet>("/mnt/c/Users/zavier/Desktop/acid/static"));
    server.getServletDispatch()->addServlet("/a",[](http::HttpRequest::ptr request
            , http::HttpResponse::ptr response
            , http::HttpSession::ptr session) ->uint32_t {
        //std::string str = request->toString();
        response->setBody("hello world");
        //response->setBody(str + "<h1>hello world</h1>");
        //response->setContentType(http::HttpContentType::TEXT_HTML);
        return 0;
    });
    
    server.getServletDispatch()->addServlet("/add",[](http::HttpRequest::ptr request
            , http::HttpResponse::ptr response
            , http::HttpSession::ptr session) ->uint32_t {
        int a = request->getParamAs("a", 0ll);
        int b = request->getParamAs("b", 0ll);
        response->setBody(std::to_string(a) + " + " + std::to_string(b) + "=" + std::to_string(a+b));
        return 0;
    });

    server.getServletDispatch()->addServlet("/json",[](http::HttpRequest::ptr request
            , http::HttpResponse::ptr response
            , http::HttpSession::ptr session) ->uint32_t {
        nlohmann::json json;
        std::stringstream ss;
        switch (request->getMethod()) {
            case http::HttpMethod::GET:
                json["bool"] = true;
                json["number"] = 114514;
                json["float"] = M_PI;
                json["string"] = "abcdefg";
                response->setJson(json);
                break;
            case http::HttpMethod::POST:
                json = request->getJson();
                ss << json;
                SPDLOG_INFO(json.type_name());
                SPDLOG_INFO(ss.str());
                response->setJson(json);
                break;
            default:
                break;
        }
        return 0;
    });

    while (!server.bind(address)){
        sleep(1);
    }

    server.start();
}