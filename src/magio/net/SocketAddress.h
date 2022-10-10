#pragma once

#include <string>

#ifdef _WIN32
#include <WinSock2.h>
#endif

namespace magio {

struct SocketAddress {
    std::string ip;
    uint_least16_t host;
    
    sockaddr_in _sockaddr;

    std::string to_string() {
        return ip + ":" + std::to_string(host);
    }

    
};

}