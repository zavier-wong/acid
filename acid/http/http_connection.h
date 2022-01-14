//
// Created by zavier on 2021/12/18.
//

#ifndef ACID_HTTP_CONNECTION_H
#define ACID_HTTP_CONNECTION_H
#include <memory>
#include "acid/net/socket_stream.h"
#include "acid/net/uri.h"
#include "http.h"
#include "acid/mutex.h"
namespace acid::http {
struct HttpResult {
    using ptr = std::shared_ptr<HttpResult>;
    enum Result{
        OK = 0,
        INVALID_URL,
        INVALID_HOST,
        CREATE_SOCKET_ERROR,
        CONNECT_FAIL,
        SEND_CLOSE_BY_PEER,
        SEND_SOCKET_ERROR,
        TIMEOUT,
        POOL_INVALID_CONNECTION,
    };
    HttpResult(Result result_, HttpResponse::ptr response_, const std::string& msg_)
        : result(result_)
        , response(response_)
        , msg(msg_){
    }
    std::string toString() const;
    Result result;
    HttpResponse::ptr response;
    std::string msg;
};
class HttpConnectionPool;
class HttpConnection : public SocketStream {
    friend HttpConnectionPool;
public:
    using ptr = std::shared_ptr<HttpConnection>;
    ~HttpConnection();
    static HttpResult::ptr DoGet(const std::string& url,
                                 uint64_t timeout_ms = -1,
                                 const std::map<std::string,std::string>& header = {},
                                 const std::string& body = "");

    static HttpResult::ptr DoGet(Uri::ptr uri,
                                  uint64_t timeout_ms = -1,
                                  const std::map<std::string,std::string>& header = {},
                                  const std::string& body = "");

    static HttpResult::ptr DoPost(const std::string& url,
                                 uint64_t timeout_ms = -1,
                                 const std::map<std::string,std::string>& header = {},
                                 const std::string& body = "");

    static HttpResult::ptr DoPost(Uri::ptr uri,
                                 uint64_t timeout_ms = -1,
                                 const std::map<std::string,std::string>& header = {},
                                 const std::string& body = "");

    static HttpResult::ptr DoRequest(HttpMethod method,
                                     const std::string& url,
                                     uint64_t timeout_ms = -1,
                                     const std::map<std::string,std::string>& header = {},
                                     const std::string& body = "");

    static HttpResult::ptr DoRequest(HttpMethod method,
                                     Uri::ptr uri,
                                     uint64_t timeout_ms = -1,
                                     const std::map<std::string,std::string>& header = {},
                                     const std::string& body = "");

    static HttpResult::ptr DoRequest(HttpRequest::ptr request, Uri::ptr uri, uint64_t timeout_ms = -1);

    HttpConnection(Socket::ptr socket, bool owner = true);

    HttpResponse::ptr recvResponse();

    ssize_t sendRequest(HttpRequest::ptr request);
private:
    uint64_t m_createTime = 0;
    uint64_t m_request = 0;
};

class HttpConnectionPool {
public:
    using ptr = std::shared_ptr<HttpConnectionPool>;
    using MutexType = Mutex;
    static HttpConnectionPool::ptr Create(const std::string& uri
            ,const std::string& vhost
            ,uint32_t max_size
            ,uint32_t max_alive_time
            ,uint32_t max_request);
    HttpConnectionPool(const std::string& host
                        ,const std::string& vhost
                        ,uint32_t port
                        ,bool is_https
                        ,uint32_t max_size
                        ,uint32_t max_alive_time
                        ,uint32_t max_request);

    HttpResult::ptr doGet(const std::string& url,
                                 uint64_t timeout_ms = -1,
                                 const std::map<std::string,std::string>& header = {},
                                 const std::string& body = "");

    HttpResult::ptr doGet(Uri::ptr uri,
                                 uint64_t timeout_ms = -1,
                                 const std::map<std::string,std::string>& header = {},
                                 const std::string& body = "");

    HttpResult::ptr doPost(const std::string& url,
                                  uint64_t timeout_ms = -1,
                                  const std::map<std::string,std::string>& header = {},
                                  const std::string& body = "");

    HttpResult::ptr doPost(Uri::ptr uri,
                                  uint64_t timeout_ms = -1,
                                  const std::map<std::string,std::string>& header = {},
                                  const std::string& body = "");

    HttpResult::ptr doRequest(HttpMethod method,
                                     const std::string& url,
                                     uint64_t timeout_ms = -1,
                                     const std::map<std::string,std::string>& header = {},
                                     const std::string& body = "");

    HttpResult::ptr doRequest(HttpMethod method,
                                     Uri::ptr uri,
                                     uint64_t timeout_ms = -1,
                                     const std::map<std::string,std::string>& header = {},
                                     const std::string& body = "");

    HttpResult::ptr doRequest(HttpRequest::ptr request, uint64_t timeout_ms = -1);

    HttpConnection::ptr getConnection();

private:
    static void RelesePtr(HttpConnection* ptr, HttpConnectionPool* pool);
private:
    std::string m_host;
    std::string m_vhost;
    uint32_t m_port;
    uint32_t m_maxSize;
    uint32_t m_maxAliveTime;
    uint32_t m_maxRequest;
    bool m_isHttps;
    MutexType m_mutex;
    std::list<HttpConnection*> m_conns;
    std::atomic<int32_t> m_total{0};
};

}

#endif //ACID_HTTP_CONNECTION_H
