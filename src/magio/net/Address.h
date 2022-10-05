#pragma once

#include <string>

namespace magio {

struct Address {
    std::string ip;
    uint_least16_t host;

    std::string to_string() {
        return ip + ":" + std::to_string(host);
    }
};

}