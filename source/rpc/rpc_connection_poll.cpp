//
// Created by zavier on 2022/1/16.
//

#include "acid/rpc/rpc_connection_poll.h"

namespace acid::rpc {

Protocol::ptr RpcConnectionPool::recvResponse() {
    SocketStream::ptr stream = std::make_shared<SocketStream>(m_registry, false);
    Protocol::ptr response = std::make_shared<Protocol>();

    ByteArray::ptr byteArray = std::make_shared<ByteArray>();

    if (stream->readFixSize(byteArray, response->BASE_LENGTH) <= 0) {
        return nullptr;
    }

    byteArray->setPosition(0);
    response->decodeMeta(byteArray);

    std::string buff;
    buff.resize(response->getContentLength());

    if (stream->readFixSize(&buff[0], buff.size()) <= 0) {
        return nullptr;
    }
    response->setContent(std::move(buff));
    return response;
}

bool RpcConnectionPool::sendRequest(Protocol::ptr p) {
    SocketStream::ptr stream = std::make_shared<SocketStream>(m_registry, false);
    ByteArray::ptr bt = p->encode();
    if (stream->writeFixSize(bt, bt->getSize()) < 0) {
        return false;
    }
    return true;
}
std::vector<std::string> RpcConnectionPool::discover(const std::string& name){
    std::vector<Result<std::string>> res;
    std::vector<std::string> rt;
    //Result<std::string> res;
    std::vector<Address::ptr> addrs;
    Protocol::ptr proto = std::make_shared<Protocol>();
    proto->setMsgType(Protocol::MsgType::RPC_SERVICE_DISCOVER);
    proto->setContent(name);

    sendRequest(proto);
    auto response = recvResponse();
    Serializer s(response->getContent());
    uint32_t cnt;
    s >> cnt;
    for (uint32_t i = 0; i < cnt; ++i) {
        Result<std::string> str;
        s >> str;
        res.push_back(str);
    }
    if (res.front().getCode() == RPC_NO_METHOD) {
        return std::vector<std::string>{};
    }
    //
    for (size_t i = 0; i < res.size(); ++i) {
        //Address::ptr addr = Address::LookupAny(res[i].getVal());
        //addr = Address::LookupAny("127.0.0.1:8080");
        //if (addr) {
            //addrs.push_back(addr);
        //}
        rt.push_back(res[i].getVal());
    }
    return rt;
}

}