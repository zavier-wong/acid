//
// Created by zavier on 2022/1/26.
//

#include "acid/lexical_cast.h"

namespace acid {
namespace detail {
    const char* strue = "true";
    const char* sfalse = "false";

    bool checkbool(const char* from, const size_t len, const char* s) {
        for (size_t i = 0; i < len; i++) {
            if (from[i] != s[i]) {
                return false;
            }
        }

        return true;
    }

    bool convert(const char* from) {
        const unsigned int len = strlen(from);
        if (len != 4 && len != 5)
            throw std::invalid_argument("argument is invalid");

        bool r = true;
        if (len == 4) {
            r = checkbool(from, len, strue);

            if (r)
                return true;
        }
        else {
            r = checkbool(from, len, sfalse);

            if (r)
                return false;
        }

        throw std::invalid_argument("argument is invalid");
    }
}

}

