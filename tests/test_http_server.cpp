//
// Created by zavier on 2021/12/15.
//
#include "acid/acid.h"
#include <iostream>
static acid::Logger::ptr g_logger = ACID_LOG_ROOT();

int main(){
    //ACID_LOG_DEBUG(g_logger) << ACID_LOG_NAME("system").get()->getLevel();
    //ACID_LOG_NAME("system").get()->setLevel(acid::LogLevel::FATAL);
    //acid::Config::LoadFromFile("../config/log.yaml");
    acid::IOManager::ptr ioManager(new acid::IOManager{4});
    ioManager->submit([]{
        acid::Address::ptr address = acid::Address::LookupAny("0.0.0.0:8081");
        acid::http::HttpServer::ptr server(new acid::http::HttpServer(true));
        server->getServletDispatch()->addGlobServlet("/*",
                     std::make_shared<acid::http::FileServlet>("/mnt/c/Users/zavier/Desktop/acid/static"));
        server->getServletDispatch()->addServlet("/a",[](acid::http::HttpRequest::ptr request
                , acid::http::HttpResponse::ptr response
                , acid::http::HttpSession::ptr session) ->uint32_t {
            //std::string str = request->toString();
            response->setBody("hello world");
            //response->setBody(str + "<h1>hello world</h1>");
            //response->setContentType(acid::http::HttpContentType::TEXT_HTML);
            return 0;
        });
        server->getServletDispatch()->addServlet("/add",[](acid::http::HttpRequest::ptr request
                , acid::http::HttpResponse::ptr response
                , acid::http::HttpSession::ptr session) ->uint32_t {
            int a = request->getParamAs("a", 0ll);
            int b = request->getParamAs("b", 0ll);
            response->setBody(std::to_string(a) + " + " + std::to_string(b) + "=" + std::to_string(a+b));
            return 0;
        });

        server->getServletDispatch()->addServlet("/json",[](acid::http::HttpRequest::ptr request
                , acid::http::HttpResponse::ptr response
                , acid::http::HttpSession::ptr session) ->uint32_t {
            acid::Json json;
            switch (request->getMethod()) {
                case acid::http::HttpMethod::GET:
                    json["bool"] = true;
                    json["number"] = 114514;
                    json["float"] = M_PI;
                    json["string"] = "abcdefg";
                    response->setJson(json);
                    break;
                case acid::http::HttpMethod::POST:
                    json = request->getJson();
                    ACID_LOG_INFO(g_logger) << json.type_name() << "\n" << json;
                    response->setJson(json);
                    break;
                default:
                    break;
            }
            return 0;
        });

        while (!server->bind(address)){
            sleep(1);
        }

        server->start();
    });
}