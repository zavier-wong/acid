//
// Created by zavier on 2021/12/17.
//

#ifndef ACID_SERVLET_H
#define ACID_SERVLET_H
#include <functional>
#include <memory>
#include <libgo/libgo.h>
#include "http.h"
#include "http_session.h"

namespace acid::http {
class Servlet {
public:
    using ptr = std::shared_ptr<Servlet>;
    Servlet(const std::string& name) : m_name(name) {};
    virtual ~Servlet() {};
    virtual int32_t handle(HttpRequest::ptr request, HttpResponse::ptr response, HttpSession::ptr session) = 0;
    const std::string& getName() const { return m_name;}
private:
    std::string m_name;
};


class FunctionServlet : public  Servlet{
public:
    using ptr = std::shared_ptr<FunctionServlet>;
    using callback = std::function<int32_t(HttpRequest::ptr request, HttpResponse::ptr response,
                                           HttpSession::ptr session)>;
    FunctionServlet(callback cb) : Servlet("FunctionServlet") , m_cb(cb) {};
    int32_t handle(HttpRequest::ptr request, HttpResponse::ptr response, HttpSession::ptr session) override;
private:
    callback m_cb;
};

class ServletDispatch : public Servlet{
public:
    using ptr = std::shared_ptr<ServletDispatch>;
    using RWMutexType = co::co_rwmutex;
    ServletDispatch();
    int32_t handle(HttpRequest::ptr request, HttpResponse::ptr response, HttpSession::ptr session) override;

    void addServlet(const std::string& uri, Servlet::ptr slt);
    void addServlet(const std::string& uri, FunctionServlet::callback cb);

    void addGlobServlet(const std::string& uri, Servlet::ptr slt);
    void addGlobServlet(const std::string& uri, FunctionServlet::callback cb);

    void delServlet(const std::string& uri);
    void delGlobServlet(const std::string& uri);

    Servlet::ptr getServlet(const std::string& uri);
    Servlet::ptr getGlobServlet(const std::string& uri);

    Servlet::ptr getMatchedServlet(const std::string& uri);

    Servlet::ptr getDefault() const { return m_default;}
    void setDefault(Servlet::ptr def) { m_default = def;}
private:
    RWMutexType m_mutex;
    std::map<std::string, Servlet::ptr> m_datas;
    std::map<std::string, Servlet::ptr> m_globs;
    Servlet::ptr m_default;
};


class NotFoundServlet : public Servlet {
public:
    using ptr = std::shared_ptr<NotFoundServlet>;
    NotFoundServlet(const std::string& name) ;
    int32_t handle(HttpRequest::ptr request, HttpResponse::ptr response, HttpSession::ptr session) override;

private:
    std::string m_name;
    std::string m_content;
};

}

#endif //ACID_SERVLET_H
