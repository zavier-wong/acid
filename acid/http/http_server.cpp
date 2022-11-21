//
// Created by zavier on 2021/12/15.
//

#include "acid/http/http_server.h"

#include <memory>
#include "acid/http/parse.h"
#include "acid/http/servlet.h"
namespace acid::http {
static auto g_logger = GetLogInstance();

HttpServer::HttpServer(bool keepalive, co::Scheduler* worker, co::Scheduler* accept_worker)
    : TcpServer(worker, accept_worker)
    , m_isKeepalive(keepalive){
    m_dispatch.reset(new ServletDispatch);

}

void HttpServer::handleClient(Socket::ptr client) {
    SPDLOG_LOGGER_DEBUG(g_logger, "handleClient: {}", client->toString());
    HttpRequestParser::ptr parser(new HttpRequestParser);
    HttpSession::ptr session(new HttpSession(client));
    while (true) {
        HttpRequest::ptr request = session->recvRequest();
        if (!request) {
            session->close();
            SPDLOG_LOGGER_DEBUG(g_logger, "recv http request fail, errno={}, errstr={}, client={}, keep_alive={}",
                                errno, strerror(errno), client->toString(), m_isKeepalive);
            break;
        }
        HttpResponse::ptr response(new HttpResponse(request->getVersion(), request->isClose() || !m_isKeepalive));
        response->setHeader("Server" ,getName());

        if (m_dispatch->handle(request, response, session) == 0) {
            session->sendResponse(response);
        }

        if (request->isClose() || !m_isKeepalive) {
            break;
        }
    }
    session->close();
}

void HttpServer::setName(const std::string& name) {
    TcpServer::setName(name);
    m_dispatch->setDefault(std::make_shared<NotFoundServlet>(name));
}

}