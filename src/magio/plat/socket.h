#pragma once

#include "magio/plat/declare.h"
#include "magio/plat/errors.h"
#include "magio/net/SocketAddress.h"
#include "magio/dev/Resource.h"

#ifdef __linux__
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h> // close
#endif

#ifdef _WIN32
#include <Ws2tcpip.h>
#include <WinSock2.h>
#endif

namespace magio {

namespace plat {

#ifdef __linux__

struct Buffer {
    char* buf;
    size_t len;
};

struct IOData {
    int     fd;
    IOOP    op;
    sockaddr_in local;
    sockaddr_in remote;
    void*   ptr;
    void(*cb)(std::error_code, void*, int);
    Buffer  wsa_buf;
};


inline SocketAddress local_address(int fd) {
    ::sockaddr_in addr{};
    ::socklen_t len = sizeof(sockaddr_in);
    ::getsockname(fd, (::sockaddr*)&addr, &len);
    return {::inet_ntoa(addr.sin_addr), ntohs(addr.sin_port)};
}

inline SocketAddress remote_address(int fd) {
    ::sockaddr_in addr{};
    ::socklen_t len = sizeof(sockaddr_in);
    ::getpeername(fd, (::sockaddr*)&addr, &len);
    return {::inet_ntoa(addr.sin_addr), ntohs(addr.sin_port)};
}

inline std::error_code set_nonblock(int fd) {
    int oldflag = ::fcntl(fd, F_GETFL, 0);
    int newflag = oldflag | O_NONBLOCK;
    if (-1 == ::fcntl(fd, F_SETFL, newflag)) {
        return std::make_error_code((std::errc)errno);
    }
    return {};
}

inline std::error_code get_error(int fd) {
    int error = 0;
    socklen_t len = sizeof(error);
    if (-1 == getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len)) {
        return MAGIO_SYSTEM_ERROR;
    }

    return make_linux_system_error(error);
}

inline socket_type make_socket(Protocol procotol) {
    MAGIO_MAKE_SOCKET;
    if(procotol == Protocol::TCP) {
        return ::socket(AF_INET, SOCK_STREAM, 0);
    }
    return ::socket(AF_INET, SOCK_DGRAM, 0);
}

inline void close_socket(int fd) {
    MAGIO_CLOSE_SOCKET;
    ::close(fd);
}
#endif

#ifdef _WIN32

struct IOData {
    OVERLAPPED overlapped;
    SOCKET fd;
    IOOP op;
    sockaddr_in local;
    sockaddr_in remote;
    void* ptr;
    void(*cb)(std::error_code, void*, SOCKET);
    WSABUF wsa_buf;
};

inline SocketAddress local_address(SOCKET fd) {
    char buff[40]{};
    ::sockaddr_in addr{};
    ::socklen_t len = sizeof(sockaddr_in);
    ::getsockname(fd, (::sockaddr*)&addr, &len);
    ::inet_ntop(addr.sin_family, &addr.sin_addr, buff, 40);
    return {buff, ::ntohs(addr.sin_port), addr};
}

inline SocketAddress remote_address(SOCKET fd) {
    char buff[40]{};
    ::sockaddr_in addr{};
    ::socklen_t len = sizeof(sockaddr_in);
    ::getpeername(fd, (::sockaddr*)&addr, &len);
    ::inet_ntop(addr.sin_family, &addr.sin_addr, buff, 40);
    return {buff, ::ntohs(addr.sin_port), addr};
}

inline sokcet_type make_socket(Protocol procotol) {
    MAGIO_MAKE_SOCKET;
    if(procotol == Protocol::TCP) {
        return ::WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, 0, 0, WSA_FLAG_OVERLAPPED);
    }
    return ::WSASocketW(AF_INET, SOCK_DGRAM, IPPROTO_UDP, 0, 0, WSA_FLAG_OVERLAPPED);
}

inline void close_socket(SOCKET sock) {
    MAGIO_CLOSE_SOCKET;
    ::closesocket(sock);
}

#endif

class WebSocket {
    WebSocket() {
#ifdef _WIN32
        WSADATA wsa_data;
        if (0 != ::WSAStartup(MAKEWORD(2, 2), &wsa_data)) {
            //
        }
#endif
    }
public:
    ~WebSocket() {
#ifdef _WIN32
        ::WSACleanup();
#endif
    }

    WebSocket(const WebSocket&) = delete;
    WebSocket(WebSocket&&) noexcept = delete;

    void static init() {
        static WebSocket ws;
    }
private:
};

}

}

