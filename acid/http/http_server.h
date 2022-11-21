//
// Created by zavier on 2021/12/15.
//

#ifndef ACID_HTTP_SERVER_H
#define ACID_HTTP_SERVER_H
#include <memory>
#include "http.h"
#include "http_session.h"
#include "servlet.h"
#include "acid/net/tcp_server.h"

namespace acid::http {
class HttpServer : public TcpServer {
public:
    HttpServer(bool keepalive = false, co::Scheduler* worker = &co_sched, co::Scheduler* accept_worker = &co_sched);
    ServletDispatch::ptr getServletDispatch() const { return m_dispatch;}
    void setServletDispatch(ServletDispatch::ptr v) { m_dispatch = v;}
    void setName(const std::string& name) override;
protected:
    void handleClient(Socket::ptr client) override;

private:
    bool m_isKeepalive;
    ServletDispatch::ptr m_dispatch;
};
}
#endif //ACID_HTTP_SERVER_H
