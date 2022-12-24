// only for .cpp
#ifndef MAGIO_NET_IO_CONTEXT_H_
#define MAGIO_NET_IO_CONTEXT_H_

#include "magio-v3/core/error.h"

#ifdef _WIN32
#include <WinSock2.h>
#include <ws2ipdef.h>
#elif defined(__linux__)

#endif

namespace magio {

namespace net {

enum class Operation {
    Accept,
    Connect,
    Receive,
    Send,
};

// for linux
struct IoBuf {
    size_t len;
    char* buf;
};

struct IoContext {
#ifdef _WIN32
    OVERLAPPED overlapped;
    Operation op;
    SOCKET socket_handle;
    union {
        sockaddr_in remote_addr;
        sockaddr_in6 remote_addr6;
    };
    WSABUF buf;
    void* ptr;
    void(*cb)(std::error_code, void*);
#elif defined(__linux__)
    Operation op;
    int socket_handle;
    IpAddress remote;
    IoBuf buf;
    void* ptr;
    void(*cb)(std::error_code, void*);
#endif

};

}

}
#endif