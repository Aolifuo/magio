// only for .cpp
#ifndef MAGIO_CORE_IO_CONTEXT_H_
#define MAGIO_CORE_IO_CONTEXT_H_

#include "magio-v3/core/error.h"

#ifdef _WIN32
#include <WinSock2.h>
#include <Ws2tcpip.h>
#elif defined(__linux__)
#include <netinet/in.h>
#endif

namespace magio {

enum class Operation {
    ReadFile,
    WriteFile,
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
    SOCKET handle;
    union {
        sockaddr_in remote_addr;
        sockaddr_in6 remote_addr6;
    };
    socklen_t addr_len;
    WSABUF buf;
    void* ptr;
    void(*cb)(std::error_code, void*);
#elif defined(__linux__)
    Operation op;
    int handle;
    union {
        sockaddr_in remote_addr;
        sockaddr_in6 remote_addr6;
    };
    socklen_t addr_len;
    IoBuf buf;
    void* ptr;
    void(*cb)(std::error_code, void*);
#endif

};

}

#endif