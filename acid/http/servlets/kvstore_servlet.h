//
// Created by zavier on 2022/12/6.
//

#ifndef ACID_KVSTORE_SERVLET_H
#define ACID_KVSTORE_SERVLET_H

#include "../servlet.h"

namespace acid::kvraft {
    class KVServer;
}
namespace acid::http {
class KVStoreServlet : public Servlet {
public:
    using ptr = std::shared_ptr<KVStoreServlet>;
    KVStoreServlet(std::shared_ptr<acid::kvraft::KVServer> store);
    /**
     * request 和 response 都以 json 来交互
     * request json 格式：
     *  {
     *      "command": "put",
     *      "key": "name",
     *      "value": "zavier"
     *  }
     * response json 格式：
     *  {
     *      "msg": "OK",
     *      "value": "" // 如果 request 的 command 是 get 请求返回对应 key 的 value， 否则为空
     *      "data": {   // 如果 request 的 command 是 dump 请求返回数据库的全部键值对
     *          "key1": "value1",
     *          "key2": "value2",
     *          ...
     *      }
     *  }
     */
    int32_t handle(HttpRequest::ptr request, HttpResponse::ptr response, HttpSession::ptr session) override;
private:
    std::shared_ptr<acid::kvraft::KVServer> m_store;
};
}
#endif //ACID_KVSTORE_SERVLET_H
