#pragma once

#include <string>
#include "magio/core/Error.h"
#include "magio/plat/errors.h"

#ifdef __linux__
#include <arpa/inet.h>
#endif

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

inline Expected<SocketAddress> make_address(const char* ip, uint_least16_t port) {
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (-1 == ::inet_pton(AF_INET, ip, &addr.sin_addr)) {
        return MAGIO_SYSTEM_ERROR;
    }
    return SocketAddress{ip, port, addr};
}

}