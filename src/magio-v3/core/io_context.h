// only for .cpp
#ifndef MAGIO_CORE_IO_CONTEXT_H_
#define MAGIO_CORE_IO_CONTEXT_H_

#include <system_error>

#include "magio-v3/core/coroutine.h"

#ifdef _WIN32
#include <WinSock2.h>
#include <Ws2tcpip.h>
#elif defined(__linux__)
#include <netinet/in.h>
#endif

namespace magio {

enum class Operation {
    WakeUp,
    ReadFile,
    WriteFile,
    Accept,
    Connect,
    Receive,
    Send,
};

// for linux
struct IoBuf {
    char* buf;
    size_t len;
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
    void(*cb)(std::error_code, IoContext*, void*);
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
    void(*cb)(std::error_code, IoContext*, void*);
#endif
};

struct ResumeHandle {
    std::error_code ec;
    std::coroutine_handle<> handle;
};

inline void completion_callback(std::error_code ec, IoContext* ioc, void* ptr) {
    auto* h = static_cast<ResumeHandle*>(ptr);
    h->ec = ec;
    h->handle.resume();
}

#ifdef __linux__
struct ResumeWithMsg {
    std::error_code ec;
    msghdr msg;
    std::coroutine_handle<> handle;
};

inline void completion_callback_with_msg(std::error_code ec, IoContext* ioc, void* ptr) {
    auto* h = static_cast<ResumeWithMsg*>(ptr);
    h->ec = ec;
    h->handle.resume();
}
#endif

}

#endif