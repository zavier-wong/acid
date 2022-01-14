//
// Created by zavier on 2022/1/13.
//

#include "acid/rpc/serializer.h"
#include "acid/rpc/rpc.h"
#include "acid/log.h"
#include <iostream>
static acid::Logger::ptr g_logger = ACID_LOG_ROOT();

int main() {

    acid::rpc::Serializer s;
    acid::rpc::Result<int> r, t;
    r.setCode(acid::rpc::RPC_SUCCESS);
    r.setVal(23);
    s << r;
    s.reset();
    s >> t;
    ACID_LOG_DEBUG(g_logger) << t.toString();
}