//
// Created by zavier on 2021/12/11.
//

#include "acid/http/http.h"
#include "acid/log.h"
namespace acid::http{
static Logger::ptr g_logger = ACID_LOG_NAME("system");

HttpMethod StringToMethod(const std::string& m){
#define XX(num, name, string)               \
    if(strcmp(m.c_str(), #string) == 0){    \
        return HttpMethod::name;            \
    }
    HTTP_METHOD_MAP(XX)
#undef XX
    return HttpMethod::INVALID_METHOD;
}

HttpMethod CharsToMethod(const char* m){
#define XX(num, name, string)               \
    if(strcmp(m, #string) == 0){    \
        return HttpMethod::name;            \
    }
    HTTP_METHOD_MAP(XX)
#undef XX
    return HttpMethod::INVALID_METHOD;
}

HttpContentType StringToContentType(const std::string& m){
#define XX(name, string)                    \
    if(strcmp(m.c_str(), #string) == 0){    \
        return HttpContentType::name;       \
    }
    HTTP_CONTENT_TYPE(XX)
#undef XX
    return HttpContentType::INVALID_TYPE;
}

HttpContentType CharsToContentType(const char* m){
#define XX(name, string)                \
    if(strcmp(m, #string) == 0){        \
        return HttpContentType::name;   \
    }
    HTTP_CONTENT_TYPE(XX)
#undef XX
    return HttpContentType::INVALID_TYPE;
}

static const char* s_method_string[] = {
#define XX(num, name, string) #string,
        HTTP_METHOD_MAP(XX)
#undef XX
};

std::string HttpMethodToString(HttpMethod m){
    uint32_t idx = (uint32_t)m;
    if(idx >= (sizeof(s_method_string) / sizeof(s_method_string[0]))) {
        return "<unknown>";
    }
    return s_method_string[idx];
}

std::string HttpStatusToString(HttpStatus s){
    switch(s) {
#define XX(code, name, msg) \
        case HttpStatus::name: \
            return #msg;
        HTTP_STATUS_MAP(XX);
#undef XX
        default:
            return "<unknown>";
    }
}

static const char* s_type_string[] = {
#define XX(name, string) #string,
        HTTP_CONTENT_TYPE(XX)
#undef XX
};

std::string HttpContentTypeToString(HttpContentType t){
    uint32_t idx = (uint32_t)t;
    if(idx >= (sizeof(s_type_string) / sizeof(s_type_string[0]))) {
        return "text/plain";
    }
    return s_type_string[idx];
}

bool CaseInsensitiveLess::operator()(const std::string &lhs, const std::string &rhs) const {
    return (strcasecmp(lhs.c_str(), rhs.c_str()) < 0);
}

HttpRequest::HttpRequest(uint8_t version, bool close)
        :m_method(HttpMethod::GET)
        ,m_version(version)
        ,m_close(close)
        ,m_websocket(false)
        ,m_path("/"){

}

Json HttpRequest::getJson() const {
    Json json;
    try {
        json = Json::parse(m_body);
    } catch (...) {
        ACID_LOG_INFO(g_logger) << "HttpRequest::getJson() fail, body=" << m_body;
    }
    return json;
}

void HttpRequest::setJson(const Json& json) {
    setContentType(HttpContentType::APPLICATION_JSON);
    m_body = json.dump();
}

std::string HttpRequest::getHeader(const std::string &key, const std::string &def) {
    auto it = m_headers.find(key);
    return it == m_headers.end()? def : it->second;
}

std::string HttpRequest::getParam(const std::string &key, const std::string &def) {
    auto it = m_params.find(key);
    return it == m_params.end()? def : it->second;
}

std::string HttpRequest::getcookie(const std::string &key, const std::string &def) {
    auto it = m_cookies.find(key);
    return it == m_cookies.end()? def : it->second;
}

void HttpRequest::setHeader(const std::string &key, const std::string &val) {
    m_headers[key] = val;
}

void HttpRequest::setParam(const std::string &key, const std::string &val) {
    m_params[key] = val;
}

void HttpRequest::setCookie(const std::string &key, const std::string &val) {
    m_cookies[key] = val;
}

void HttpRequest::delHeader(const std::string &key) {
    m_headers.erase(key);
}

void HttpRequest::delParam(const std::string &key) {
    m_params.erase(key);
}

void HttpRequest::delCookie(const std::string &key) {
    m_cookies.erase(key);
}

bool HttpRequest::hasHeader(const std::string &key, std::string *val) {
    auto it = m_headers.find(key);
    if(it == m_headers.end()) {
        return false;
    }
    val = &it->second;
    return true;
}

bool HttpRequest::hasParam(const std::string &key, std::string *val) {
    auto it = m_params.find(key);
    if(it == m_params.end()) {
        return false;
    }
    val = &it->second;
    return true;
}

bool HttpRequest::hasCookie(const std::string &key, std::string *val) {
    auto it = m_cookies.find(key);
    if(it == m_cookies.end()) {
        return false;
    }
    val = &it->second;
    return true;
}

std::ostream &HttpRequest::dump(std::ostream& os) const{
    // GET /index.html?a=b#x HTTP/1.1
    // Host : vvip.icu
    os  << HttpMethodToString(m_method) << " "
        << m_path
        << (m_query.empty() ? "" : "?")
        << m_query
        << (m_fragment.empty() ? "" : "#")
        << " HTTP/"
        << ((uint32_t)m_version >> 4)
        << "."
        << ((uint32_t)m_version & 0xF)
        << "\r\n";

    if(!m_websocket){
        os << "connection: " << (m_close ? "close" : "keep-alive") << "\r\n";
    }
    for(auto& i : m_headers) {
        if(!m_websocket && strcasecmp(i.first.c_str(), "connection") == 0) {
            continue;
        }
        os << i.first << ": " << i.second << "\r\n";
    }

    // TODO cookie

    if(m_body.empty()){
        os << "\r\n";
    } else {
        os << "content-length: " << m_body.size()
            << "\r\n\r\n"
            << m_body;
    }

    return os;
}

std::string HttpRequest::toString() const{
    std::stringstream ss;
    dump(ss);
    return ss.str();
}

HttpResponse::HttpResponse(uint8_t version, bool close)
    :m_status(HttpStatus::OK)
    ,m_version(version)
    ,m_close(close)
    ,m_websocket(false){
}

Json HttpResponse::getJson() const {
    Json json;
    try {
        json = Json::parse(m_body);
    } catch (...) {
        ACID_LOG_INFO(g_logger) << "HttpResponse::getJson() fail, body=" << m_body;
    }
    return json;
}

void HttpResponse::setJson(const Json& json) {
    setContentType(HttpContentType::APPLICATION_JSON);
    m_body = json.dump();
}

const std::string& HttpResponse::getHeader(const std::string& key, const std::string& def) const{
    auto it = m_headers.find(key);
    return (it == m_headers.end() ? def : it->second);
}
void HttpResponse::setHeader(const std::string& key, const std::string& val) {
    m_headers[key] = val;
}
void HttpResponse::delHeader(const std::string& key) {
    m_headers.erase(key);
}
bool HttpResponse::hasHeader(const std::string& key, std::string* val) {
    auto it = m_headers.find(key);
    if(it == m_headers.end()){
        return false;
    }
    val = &it->second;
    return true;
}
std::ostream& HttpResponse::dump(std::ostream& os) const {
    // HTTP/1.1 200 OK
    // server: acid
    os  << "HTTP/"
        << ((uint32_t)m_version >> 4)
        << "."
        << ((uint32_t)m_version & 0xF)
        << " "
        << (uint32_t)m_status
        << " "
        << (m_reason.empty() ? HttpStatusToString(m_status) : m_reason)
        << "\r\n";
    if(!m_websocket){
        os << "connection: " << (m_close ? "close" : "keep-alive") << "\r\n";
    }
    for(auto& i : m_headers) {
        if(!m_websocket && strcasecmp(i.first.c_str(), "connection") == 0) {
            continue;
        }
        os << i.first << ": " << i.second << "\r\n";
    }
    for(auto& i : m_cookies) {
        os << "Set-Cookie: " << i << "\r\n";
    }

    // TODO cookie

    if(m_body.empty()){
        os << "\r\n";
    } else {
        if (getHeader("content-length","").empty()) {
            os << "content-length: " << m_body.size();
        }
        os  << "\r\n\r\n" << m_body;
    }
    return os;
}
std::string HttpResponse::toString() const {
    std::stringstream ss;
    dump(ss);
    return ss.str();
}

}