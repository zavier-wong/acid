//
// Created by zavier on 2022/1/13.
//

#ifndef ACID_RPC_H
#define ACID_RPC_H
#include <string>
#include <map>
#include <string>
#include <sstream>
#include <functional>
#include <memory>
#include "serializer.h"

namespace acid::rpc {

template<typename T>
struct return_type{	typedef T type; };

template<>
struct return_type<void>{ typedef int8_t type; };

/**
 * @brief 调用结果为 void 类型的，将类型转换为 int8_t
 */
template<typename T>
using return_type_t = typename return_type<T>::type;


/**
 * @brief RPC调用状态
 */
enum RpcState{
    RPC_SUCCESS = 0,    // 成功
    RPC_FAIL,           // 失败
    RPC_NO_METHOD,      // 没有找到调用函数
    RPC_CLOSED,         // RPC 连接被关闭
    RPC_TIMEOUT         // RPC 调用超时
};

/**
 * @brief 包装 RPC调用结果
 */
template<typename T = void>
class Result{
public:
    using type = return_type_t<T>;
    using msg_type = std::string;
    using code_type = uint16_t;

    static Result<T> Success() {
        Result<T> res;
        res.setCode(RPC_SUCCESS);
        res.setMsg("success");
        return res;
    }
    static Result<T> Fail() {
        Result<T> res;
        res.setCode(RPC_FAIL);
        res.setMsg("fail");
        return res;
    }

    Result() {}

    bool valid() { return (m_code == 0 ? true : false); }

    type getVal() { return m_val; }
    void setVal(const type& val) { m_val = val; }
    void setCode(code_type code) { m_code = code; }
    int getCode() { return m_code; }
    void setMsg(msg_type msg) { m_msg = msg; }
    const msg_type& getMsg() { return m_msg; }

    /**
     * @brief 调试使用 ！！！！！！
     */
    std::string toString() {
        std::stringstream ss;
        ss << "[ code=" << m_code << " msg=" << m_msg << " val=" << m_val << " ]";
        return ss.str();
    }

    /**
     * @brief 反序列化回 Result
     * @param[in] in 序列化的结果
     * @param[in] d 反序列化回 Result
     * @return Serializer
     */
    friend Serializer& operator >> (Serializer& in, Result<T>& d) {
        in >> d.m_code >> d.m_msg;
        if (d.m_code == 0) {
            in >> d.m_val;
        }
        return in;
    }

    /**
     * @brief 将 Result 序列化
     * @param[in] out 序列化输出
     * @param[in] d 将 Result 序列化
     * @return Serializer
     */
    friend Serializer& operator << (Serializer& out, Result<T> d) {
        out << d.m_code << d.m_msg << d.m_val;
        return out;
    }
private:
    /// 调用状态
    code_type m_code = 0;
    /// 调用消息
    msg_type m_msg;
    /// 调用结果
    type m_val;
};

}
#endif //ACID_RPC_H
