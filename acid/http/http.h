//
// Created by zavier on 2021/12/11.
//

#ifndef ACID_HTTP_H
#define ACID_HTTP_H

#include <memory>
#include <string>
#include <map>
#include <vector>
#include <iostream>
#include <sstream>
#include <boost/lexical_cast.hpp>

namespace acid::http{

/* Request Methods */
#define HTTP_METHOD_MAP(XX)         \
  XX(0,  DELETE,      DELETE)       \
  XX(1,  GET,         GET)          \
  XX(2,  HEAD,        HEAD)         \
  XX(3,  POST,        POST)         \
  XX(4,  PUT,         PUT)          \
  /* pathological */                \
  XX(5,  CONNECT,     CONNECT)      \
  XX(6,  OPTIONS,     OPTIONS)      \
  XX(7,  TRACE,       TRACE)        \
  /* WebDAV */                      \
  XX(8,  COPY,        COPY)         \
  XX(9,  LOCK,        LOCK)         \
  XX(10, MKCOL,       MKCOL)        \
  XX(11, MOVE,        MOVE)         \
  XX(12, PROPFIND,    PROPFIND)     \
  XX(13, PROPPATCH,   PROPPATCH)    \
  XX(14, SEARCH,      SEARCH)       \
  XX(15, UNLOCK,      UNLOCK)       \
  XX(16, BIND,        BIND)         \
  XX(17, REBIND,      REBIND)       \
  XX(18, UNBIND,      UNBIND)       \
  XX(19, ACL,         ACL)          \
  /* subversion */                  \
  XX(20, REPORT,      REPORT)       \
  XX(21, MKACTIVITY,  MKACTIVITY)   \
  XX(22, CHECKOUT,    CHECKOUT)     \
  XX(23, MERGE,       MERGE)        \
  /* upnp */                        \
  XX(24, MSEARCH,     M-SEARCH)     \
  XX(25, NOTIFY,      NOTIFY)       \
  XX(26, SUBSCRIBE,   SUBSCRIBE)    \
  XX(27, UNSUBSCRIBE, UNSUBSCRIBE)  \
  /* RFC-5789 */                    \
  XX(28, PATCH,       PATCH)        \
  XX(29, PURGE,       PURGE)        \
  /* CalDAV */                      \
  XX(30, MKCALENDAR,  MKCALENDAR)   \
  /* RFC-2068, section 19.6.1.2 */  \
  XX(31, LINK,        LINK)         \
  XX(32, UNLINK,      UNLINK)       \
  /* icecast */                     \
  XX(33, SOURCE,      SOURCE)       \

/* Status Codes */
#define HTTP_STATUS_MAP(XX)                                                 \
  XX(100, CONTINUE,                        Continue)                        \
  XX(101, SWITCHING_PROTOCOLS,             Switching Protocols)             \
  XX(102, PROCESSING,                      Processing)                      \
  XX(200, OK,                              OK)                              \
  XX(201, CREATED,                         Created)                         \
  XX(202, ACCEPTED,                        Accepted)                        \
  XX(203, NON_AUTHORITATIVE_INFORMATION,   Non-Authoritative Information)   \
  XX(204, NO_CONTENT,                      No Content)                      \
  XX(205, RESET_CONTENT,                   Reset Content)                   \
  XX(206, PARTIAL_CONTENT,                 Partial Content)                 \
  XX(207, MULTI_STATUS,                    Multi-Status)                    \
  XX(208, ALREADY_REPORTED,                Already Reported)                \
  XX(226, IM_USED,                         IM Used)                         \
  XX(300, MULTIPLE_CHOICES,                Multiple Choices)                \
  XX(301, MOVED_PERMANENTLY,               Moved Permanently)               \
  XX(302, FOUND,                           Found)                           \
  XX(303, SEE_OTHER,                       See Other)                       \
  XX(304, NOT_MODIFIED,                    Not Modified)                    \
  XX(305, USE_PROXY,                       Use Proxy)                       \
  XX(307, TEMPORARY_REDIRECT,              Temporary Redirect)              \
  XX(308, PERMANENT_REDIRECT,              Permanent Redirect)              \
  XX(400, BAD_REQUEST,                     Bad Request)                     \
  XX(401, UNAUTHORIZED,                    Unauthorized)                    \
  XX(402, PAYMENT_REQUIRED,                Payment Required)                \
  XX(403, FORBIDDEN,                       Forbidden)                       \
  XX(404, NOT_FOUND,                       Not Found)                       \
  XX(405, METHOD_NOT_ALLOWED,              Method Not Allowed)              \
  XX(406, NOT_ACCEPTABLE,                  Not Acceptable)                  \
  XX(407, PROXY_AUTHENTICATION_REQUIRED,   Proxy Authentication Required)   \
  XX(408, REQUEST_TIMEOUT,                 Request Timeout)                 \
  XX(409, CONFLICT,                        Conflict)                        \
  XX(410, GONE,                            Gone)                            \
  XX(411, LENGTH_REQUIRED,                 Length Required)                 \
  XX(412, PRECONDITION_FAILED,             Precondition Failed)             \
  XX(413, PAYLOAD_TOO_LARGE,               Payload Too Large)               \
  XX(414, URI_TOO_LONG,                    URI Too Long)                    \
  XX(415, UNSUPPORTED_MEDIA_TYPE,          Unsupported Media Type)          \
  XX(416, RANGE_NOT_SATISFIABLE,           Range Not Satisfiable)           \
  XX(417, EXPECTATION_FAILED,              Expectation Failed)              \
  XX(421, MISDIRECTED_REQUEST,             Misdirected Request)             \
  XX(422, UNPROCESSABLE_ENTITY,            Unprocessable Entity)            \
  XX(423, LOCKED,                          Locked)                          \
  XX(424, FAILED_DEPENDENCY,               Failed Dependency)               \
  XX(426, UPGRADE_REQUIRED,                Upgrade Required)                \
  XX(428, PRECONDITION_REQUIRED,           Precondition Required)           \
  XX(429, TOO_MANY_REQUESTS,               Too Many Requests)               \
  XX(431, REQUEST_HEADER_FIELDS_TOO_LARGE, Request Header Fields Too Large) \
  XX(451, UNAVAILABLE_FOR_LEGAL_REASONS,   Unavailable For Legal Reasons)   \
  XX(500, INTERNAL_SERVER_ERROR,           Internal Server Error)           \
  XX(501, NOT_IMPLEMENTED,                 Not Implemented)                 \
  XX(502, BAD_GATEWAY,                     Bad Gateway)                     \
  XX(503, SERVICE_UNAVAILABLE,             Service Unavailable)             \
  XX(504, GATEWAY_TIMEOUT,                 Gateway Timeout)                 \
  XX(505, HTTP_VERSION_NOT_SUPPORTED,      HTTP Version Not Supported)      \
  XX(506, VARIANT_ALSO_NEGOTIATES,         Variant Also Negotiates)         \
  XX(507, INSUFFICIENT_STORAGE,            Insufficient Storage)            \
  XX(508, LOOP_DETECTED,                   Loop Detected)                   \
  XX(510, NOT_EXTENDED,                    Not Extended)                    \
  XX(511, NETWORK_AUTHENTICATION_REQUIRED, Network Authentication Required) \

/* Content Type */
#define HTTP_CONTENT_TYPE(XX)                               \
  XX(TEXT_HTML,                 text/html)                  \
  XX(TEXT_PLAIN,                text/plain)                 \
  XX(TEXT_XML,                  text/xml)                   \
  XX(IMAGE_GIF,                 image/gif)                  \
  XX(IMAGE_JPEG,                image/jpeg)                 \
  XX(IMAGE_PNG,                 image/png)                  \
  XX(APPLICATION_XHTML,         application/xhtml+xml)      \
  XX(APPLICATION_ATOM,          application/atom+xml)       \
  XX(APPLICATION_JSON,          application/json)           \
  XX(APPLICATION_PDF,           application/pdf)            \
  XX(APPLICATION_MSWORD,        application/msword)         \
  XX(APPLICATION_STREAM,        application/octet-stream)   \
  XX(APPLICATION_URLENCODED,    application/x-www-form-urlencoded) \
  XX(APPLICATION_FORM_DATA,     application/form-data)      \

/**
 * @brief HTTP方法枚举
 */
enum class HttpMethod {
#define XX(num, name, string) name = num,
    HTTP_METHOD_MAP(XX)
#undef XX
    INVALID_METHOD
};

/**
 * @brief HTTP状态枚举
 */
enum class HttpStatus {
#define XX(code, name, desc) name = code,
    HTTP_STATUS_MAP(XX)
#undef XX
};

/**
 * @brief HTTP Content-Type 枚举
 */
enum class HttpContentType {
#define XX(name, desc) name,
    HTTP_CONTENT_TYPE(XX)
#undef XX
        INVALID_TYPE
};

HttpMethod StringToMethod(const std::string& m);
HttpMethod CharsToMethod(const char* m);

HttpContentType StringToContentType(const std::string& m);
HttpContentType CharsToContentType(const char* m);


std::string HttpMethodToString(HttpMethod m);
std::string HttpStatusToString(HttpStatus s);
std::string HttpContentTypeToString(HttpContentType t);

struct CaseInsensitiveLess {
    /**
     * @brief 忽略大小写比较字符串
     */
    bool operator()(const std::string& lhs, const std::string& rhs) const;
};

template<class MapType, class T>
static T GetAs(const MapType& m, const std::string& key, const T def = T()){
    auto it = m.find(key);
    if(it == m.end()){
        return def;
    }
    try {
        return boost::lexical_cast<T>(it->second);
    } catch (...) {
        return def;
    }
}


template<class MapType, class T>
static bool CheckAndGetAs(const MapType& m, const std::string& key, T& val, const T def = T()){
    auto it = m.find(key);
    if(it == m.end()){
        val = def;
        return false;
    }
    try {
        val = boost::lexical_cast<T>(it.second);
        return true;
    } catch (...) {
        val = def;
        return false;
    }
}

class HttpRequest{
public:
    using ptr = std::shared_ptr<HttpRequest>;
    using MapType = std::map<std::string, std::string, CaseInsensitiveLess>;

    HttpRequest(uint8_t version = 0x11, bool close = true);

    HttpMethod getMethod() const { return m_method;}
    uint8_t getVersion() const { return m_version;}
    bool isClose() const { return m_close;}
    bool isWebsocket() const { return m_websocket;}
    const std::string& getPath() const { return m_path;}
    const std::string& getQuery() const { return m_query;}
    const std::string& getFragment() const { return m_fragment;}
    const std::string& getBody() const { return m_body;}
    const MapType& getHeaders() const { return m_headers;}
    const MapType& getParams() const { return m_params;}
    const MapType& getCookies() const { return m_cookies;}

    void setMethod(HttpMethod method) { m_method = method;}
    void setVersion(uint8_t version){ m_version = version;}
    void setClose(bool b) { m_close = b;}
    void setWebSocket(bool b) { m_websocket = b;}
    void setPath(const std::string& path) { m_path = path;}
    void setQuery(const std::string& query) { m_query = query;}
    void setFragment(const std::string& fragment) { m_fragment = fragment;}
    void setBody(const std::string& body) { m_body = body;}
    void setHeaders(const MapType& headers) { m_headers = headers;}
    void setParams(const MapType& params) { m_params = params;}
    void setCookies(const MapType& cookies) { m_cookies = cookies;}
    void setContentType(HttpContentType type) { setHeader("Content-Type", HttpContentTypeToString(type));}

    std::string getHeader(const std::string& key, const std::string& def = "");
    std::string getParam(const std::string& key, const std::string& def = "");
    std::string getcookie(const std::string& key, const std::string& def = "");
    HttpContentType getContentType() { return StringToContentType(getHeader("Content-Type", ""));};

    void setHeader(const std::string& key, const std::string& val);
    void setParam(const std::string& key, const std::string& val);
    void setCookie(const std::string& key, const std::string& val);

    void delHeader(const std::string& key);
    void delParam(const std::string& key);
    void delCookie(const std::string& key);

    bool hasHeader(const std::string& key, std::string* val = nullptr);
    bool hasParam(const std::string& key, std::string* val = nullptr);
    bool hasCookie(const std::string& key, std::string* val = nullptr);

    template<class T>
    T getHeaderAs(const std::string& key, const T& def = T()){
        return GetAs(m_headers, key, def);
    }
    template<class T>
    T getParamAs(const std::string& key, const T& def = T()){
        return GetAs(m_params, key, def);
    }
    template<class T>
    T getCookieAs(const std::string& key, const T& def = T()){
        return GetAs(m_cookies, key, def);
    }

    template<class T>
    bool checkAndGetHeaderAs(const std::string& key, T& val,const T& def = T()){
        return CheckAndGetAs(m_headers, key, val, def);
    }
    template<class T>
    bool checkAndGetParamAs(const std::string& key, T& val, const T& def = T()){
        return CheckAndGetAs(m_params, key, val, def);
    }
    template<class T>
    bool checkAndGetCookieAs(const std::string& key, T& val, const T& def = T()){
        return CheckAndGetAs(m_cookies, key, val, def);
    }
    std::ostream& dump(std::ostream& os) const;
    std::string toString() const;
private:
    /// 请求方法
    HttpMethod m_method;
    /// HTTP版本
    uint8_t m_version;
    /// 是否主动关闭
    bool m_close;
    /// 是否为websocket
    bool m_websocket;
    /// 请求路径
    std::string m_path;
    /// 请求参数
    std::string m_query;
    /// 请求fragment
    std::string m_fragment;
    /// 请求体
    std::string m_body;
    /// 请求头map
    MapType m_headers;
    /// 请求参数map
    MapType m_params;
    /// 请求cookies map
    MapType m_cookies;
};

class HttpResponse {
public:
    using ptr = std::shared_ptr<HttpResponse>;
    using MapType = std::map<std::string, std::string, CaseInsensitiveLess>;

    HttpResponse(uint8_t version = 0x11, bool close = true);

    HttpStatus getStatus() const { return m_status;}
    uint8_t getVersion() const { return m_version;}
    bool isClose() const { return m_close;}
    bool isWebsocket() const { return m_websocket;}
    const std::string& getBody() const { return m_body;}
    const std::string& getReason() const { return m_reason;}
    const MapType& getHeaders() const { return m_headers;}
    const std::vector<std::string> getCookies() const { return m_cookies;}
    HttpContentType getContentType() { return StringToContentType(getHeader("Content-Type", ""));};

    void setStatus(HttpStatus status) { m_status = status;}
    void setStatus(uint32_t status) { m_status = (HttpStatus)status;}
    void setVersion(uint8_t version) { m_version = version;}
    void setClose(bool b) { m_close = b;}
    void setWebsocket(bool b) { m_websocket = b;}
    void setBody(const std::string& body) { m_body = body;}
    void setReason(const std::string& reason) { m_reason = reason;}
    void setHeaders(const MapType& headers) { m_headers = headers;}
    void setCookies(const std::vector<std::string>& cookies) { m_cookies = cookies;}
    void setContentType(HttpContentType type) { setHeader("Content-Type", HttpContentTypeToString(type));}

    const std::string& getHeader(const std::string& key, const std::string& def = "") const;
    void setHeader(const std::string& key, const std::string& val);
    void delHeader(const std::string& key);
    bool hasHeader(const std::string& key, std::string* val = nullptr);
    template<class T>
    T getHeaderAs(const std::string& key, const T& def = T()){
        return GetAs(m_headers, key, def);
    }
    template<class T>
    bool checkAndGetHeaderAs(const std::string& key, T& val,const T& def = T()){
        return CheckAndGetAs(m_headers, key, val, def);
    }
    std::ostream& dump(std::ostream& os) const;
    std::string toString() const;
private:
    /// 响应状态
    HttpStatus m_status;
    /// 版本
    uint8_t m_version;
    /// 是否自动关闭
    bool m_close;
    /// 是否为websocket
    bool m_websocket;
    /// 响应消息体
    std::string m_body;
    /// 响应原因
    std::string m_reason;
    /// 响应头部MAP
    MapType m_headers;
    std::vector<std::string> m_cookies;
};

};
#endif //ACID_HTTP_H
