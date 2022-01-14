//
// Created by zavier on 2021/12/17.
//

#include <fnmatch.h>
#include "acid/http/servlet.h"

namespace acid::http {

int32_t FunctionServlet::handle(HttpRequest::ptr request, HttpResponse::ptr response, HttpSession::ptr session) {
    return m_cb(request, response, session);
}


NotFoundServlet::NotFoundServlet(const std::string &name)
    : Servlet("NotFoundServlet")
    , m_name(name){
    m_content = "<html>"
                "<head><title>404 Not Found</title></head>"
                "<body>"
                "<center><h1>404 Not Found</h1></center>"
                "<hr><center>" + m_name + "</center>"
                "</body>"
                "</html>";
}

int32_t NotFoundServlet::handle(HttpRequest::ptr request, HttpResponse::ptr response, HttpSession::ptr session) {
    response->setStatus(http::HttpStatus::NOT_FOUND);
    response->setHeader("Content-Type","text/html");
    response->setHeader("Server", "acid/1.0.0");
    response->setBody(m_content);
    return 0;
}


ServletDispatch::ServletDispatch() : Servlet("ServletDispatch") {
    m_default.reset(new NotFoundServlet("acid/1.0.0"));
}

int32_t ServletDispatch::handle(HttpRequest::ptr request, HttpResponse::ptr response, HttpSession::ptr session) {
    auto slt = getMatchedServlet(request->getPath());
    if(slt) {
        slt->handle(request, response, session);
    }
    return 0;
}

void ServletDispatch::addServlet(const std::string &uri, Servlet::ptr slt) {
    RWMutexType::WriteLock lock(m_mutex);
    m_datas[uri] = slt;
}

void ServletDispatch::addServlet(const std::string &uri, FunctionServlet::callback cb) {
    RWMutexType::WriteLock lock(m_mutex);
    m_datas[uri] = FunctionServlet::ptr(new FunctionServlet(cb));
}

void ServletDispatch::addGlobServlet(const std::string &uri, Servlet::ptr slt) {
    RWMutexType::WriteLock lock(m_mutex);
    m_globs[uri] = slt;
}

void ServletDispatch::addGlobServlet(const std::string &uri, FunctionServlet::callback cb) {
    RWMutexType::WriteLock lock(m_mutex);
    m_globs[uri] = FunctionServlet::ptr(new FunctionServlet(cb));
}

void ServletDispatch::delServlet(const std::string &uri) {
    RWMutexType::WriteLock lock(m_mutex);
    m_datas.erase(uri);
}

void ServletDispatch::delGlobServlet(const std::string &uri) {
    RWMutexType::WriteLock lock(m_mutex);
    m_globs.erase(uri);
}

Servlet::ptr ServletDispatch::getServlet(const std::string &uri) {
    RWMutexType::WriteLock lock(m_mutex);
    auto it = m_datas.find(uri);
    if(it == m_datas.end()) {
        return nullptr;
    }
    return it->second;
}

Servlet::ptr ServletDispatch::getGlobServlet(const std::string &uri) {
    RWMutexType::WriteLock lock(m_mutex);
    auto it = m_globs.find(uri);
    if(it == m_globs.end()) {
        return nullptr;
    }
    return it->second;
}

Servlet::ptr ServletDispatch::getMatchedServlet(const std::string &uri) {
    RWMutexType::WriteLock lock(m_mutex);
    auto mit = m_datas.find(uri);
    if(mit != m_datas.end()) {
        return mit->second;
    }
    for(auto it = m_globs.begin();
        it != m_globs.end(); ++it) {
        if(!fnmatch(it->first.c_str(), uri.c_str(), 0)) {
            return it->second;
        }
    }
    return m_default;
}


}