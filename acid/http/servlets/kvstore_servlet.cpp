//
// Created by zavier on 2022/12/6.
//

#include "kvstore_servlet.h"
#include "../../kvraft/kvserver.h"

namespace acid::http {
namespace {
struct Params {
    Params(const nlohmann::json& json) {
        auto it = json.find("command");
        if (it == json.end()) {
            return;
        }
        command = it->get<std::string>();

        it = json.find("key");
        if (it == json.end()) {
            return;
        }
        key = it->get<std::string>();

        it = json.find("value");
        if (it == json.end()) {
            return;
        }
        value = it->get<std::string>();
    }
    std::optional<std::string> command;
    std::optional<std::string> key;
    std::optional<std::string> value;
};
}
KVStoreServlet::KVStoreServlet(std::shared_ptr<acid::kvraft::KVServer> store)
    : Servlet("KVStoreServlet"), m_store(std::move(store)) {
    if (!m_store) {
        exit(EXIT_FAILURE);
    }
}

int32_t KVStoreServlet::handle(HttpRequest::ptr request, HttpResponse::ptr response, HttpSession::ptr session) {
    nlohmann::json req = request->getJson();
    nlohmann::json resp;
    co_defer_scope {
        response->setJson(resp);
    };

    // 允许跨域请求
    response->setHeader("Access-Control-Allow-Origin", "*");
    response->setHeader("Access-Control-Allow-Methods", "*");
    response->setHeader("Access-Control-Max-Age", "3600");
    response->setHeader("Access-Control-Allow-Headers", "*");

    Params params(req);

    if (!params.command) {
        resp["msg"] = "The request is missing a command";
        return 0;
    }

    const std::string& command = *params.command;

    if (command == "dump") {
        const auto& data = m_store->getData();
        resp["data"] = data;
        resp["msg"] = "OK";
        return 0;
    } else if (command == "clear") {
        kvraft::CommandResponse commandResponse = m_store->Clear();
        resp["msg"] = kvraft::toString(commandResponse.error);
        return 0;
    }

    if (!params.key) {
        resp["msg"] = "The request is missing a key";
        return 0;
    }
    const std::string& key = *params.key;
    kvraft::CommandResponse commandResponse;
    if (command == "get") {
        commandResponse = m_store->Get(key);
        resp["msg"] = kvraft::toString(commandResponse.error);
        resp["value"] = commandResponse.value;
    } else if (command == "delete") {
        commandResponse = m_store->Delete(key);
        resp["msg"] = kvraft::toString(commandResponse.error);
    } else if (command == "put") {
        if (!params.value) {
            resp["msg"] = "The request is missing a value";
            return 0;
        }
        const std::string& value = *params.value;
        commandResponse = m_store->Put(key, value);
        resp["msg"] = kvraft::toString(commandResponse.error);
    } else if (command == "append") {
        if (!params.value) {
            resp["msg"] = "The request is missing a value";
            return 0;
        }
        const std::string& value = *params.value;
        commandResponse = m_store->Append(key, value);
        resp["msg"] = kvraft::toString(commandResponse.error);
    } else {
        resp["msg"] = "command not allowed";
    }
    return 0;
}

}
