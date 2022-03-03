//
// Created by zavier on 2021/12/13.
//

#ifndef ACID_PARSE_H
#define ACID_PARSE_H
#include <memory>
#include "acid/coroutine.h"
#include "http.h"
//#include "io_manager.h"
#include "acid/sync.h"
namespace acid::http{
/**
 * @brief HTTP解析抽象类
 */
class HttpParser{
public:
    /// 错误码
    enum Error {
        NO_ERROR = 0,
        INVALID_METHOD,
        INVALID_PATH,
        INVALID_VERSION,
        INVALID_LINE,
        INVALID_HEADER,
        INVALID_CODE,
        INVALID_REASON,
        INVALID_CHUNK
    };
    enum CheckState{
        NO_CHECK,
        CHECK_LINE,
        CHECK_HEADER,
        CHECK_CHUNK
    };
    /**
     * @brief 构造函数
     */
    HttpParser();

    /**
     * @brief 析构函数
     */
    virtual ~HttpParser();

    /**
     * @brief 解析协议
     * @param data 协议文本内存
     * @param len 协议文本内存长度
     * @param chunk 标识是否解析 http chunk
     * @return 返回实际解析的长度,并且将已解析的数据移除
     */
    size_t execute(char* data, size_t len, bool chunk = false);
    /**
     * @brief 是否解析完成
     * @return 是否解析完成
     */
    int isFinished();

    /**
     * @brief 是否有错误
     * @return 是否有错误
     */
    int hasError();

    /**
     * @brief 设置错误
     * @param[in] v 错误值
     */
    void setError(Error v) { m_error = v;}

protected:

    virtual Task<Error> parse_line() = 0;

    virtual Task<Error> parse_header() = 0;

    virtual Task<Error> parse_chunk() = 0;

protected:
    int m_error = 0;
    bool m_finish = false;
    char* m_cur = nullptr;

    //acid::IOManager* m_mgr = nullptr;
    //acid::Fiber::ptr m_executor;
    //acid::Fiber::ptr m_parser;
    Task<Error> m_parser{};
    CheckState m_checkState = NO_CHECK;
};

/**
 * @brief HTTP请求解析类
 */
class HttpRequestParser : public HttpParser{
public:
    using ptr = std::shared_ptr<HttpRequestParser>;
    HttpRequestParser() : HttpParser(), m_data(new HttpRequest) {}


    /**
     * @brief 返回HttpRequest结构体
     */
    HttpRequest::ptr getData() const { return m_data;}

    /**
     * @brief 获取消息体长度
     */
    uint64_t getContentLength();
public:
    /**
     * @brief 返回HttpRequest协议解析的缓存大小
     */
    static uint64_t GetHttpRequestBufferSize();

    /**
     * @brief 返回HttpRequest协议的最大消息体大小
     */
    static uint64_t GetHttpRequestMaxBodySize();

private:

    void on_request_method(const std::string& str);

    void on_request_path(const std::string& str);

    void on_request_query(const std::string& str);

    void on_request_fragment(const std::string& str) ;

    void on_request_version(const std::string& str);

    void on_request_header(const std::string& key, const std::string& val);

    void on_request_header_done();

protected:
    Task<Error> parse_line() override;

    Task<Error> parse_header() override;

    Task<Error> parse_chunk() override;

private:
    /// HttpRequest结构
    HttpRequest::ptr m_data;
};

/**
 * @brief HTTP响应解析类
 */
class HttpResponseParser : public HttpParser{
public:
    using ptr = std::shared_ptr<HttpResponseParser>;
    HttpResponseParser() : HttpParser(), m_data(new HttpResponse) {}

    /**
     * @brief 返回HttpRequest结构体
     */
    HttpResponse::ptr getData() const { return m_data;}

    /**
     * @brief 获取消息体长度
     */
    uint64_t getContentLength();

    /**
     * @brief 获取消息体是否为 chunk 格式
     */
    bool isChunked();
public:
    /**
     * @brief 返回HttpRequest协议解析的缓存大小
     */
    static uint64_t GetHttpResponseBufferSize();

    /**
     * @brief 返回HttpRequest协议的最大消息体大小
     */
    static uint64_t GetHttpResponseMaxBodySize();

private:

    void on_response_version(const std::string& str);

    void on_response_status(const std::string& str);

    void on_response_reason(const std::string& str);

    void on_response_header(const std::string& key, const std::string& val);

    void on_response_header_done();

    void on_response_chunk(const std::string& str);

protected:
    Task<Error> parse_line() override;

    Task<Error> parse_header() override;

    Task<Error> parse_chunk() override;

private:
    /// HttpRequest结构
    HttpResponse::ptr m_data;
};


}

#endif //ACID_PARSE_H
