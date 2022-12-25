#include "magio-v3/net/socket.h"

#include "magio-v3/core/error.h"
#include "magio-v3/core/coro_context.h"
#include "magio-v3/core/io_context.h"
#include "magio-v3/core/detail/completion_callback.h"

#ifdef _WIN32
#include <ws2ipdef.h>
#elif defined(__linux__)
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

namespace magio {

namespace net {

namespace detail {

Socket::Handle open_socket(Ip ip, Transport tp, std::error_code& ec) {
    Socket::Handle handle;
    int af = ip == Ip::v4 ? AF_INET : AF_INET6;
    
#ifdef _WIN32
    switch(tp) {
    case Transport::Tcp:
        handle = ::WSASocketW(
            af, SOCK_STREAM, IPPROTO_TCP,
            0, 0, WSA_FLAG_OVERLAPPED
        );
        break;
    case Transport::Udp:
        handle = ::WSASocketW(
            AF_INET, SOCK_DGRAM, IPPROTO_UDP,
            0, 0, WSA_FLAG_OVERLAPPED
        );
        break;
    }
#elif defined(__linux__)
    switch(tp) {
    case Transport::Tcp:
        handle = ::socket(af, SOCK_STREAM, 0);
        break;
    case Transport::Udp:
        handle = ::socket(af, SOCK_DGRAM, 0);
        break;
    }
#endif

    if (handle == -1) {
        ec = SOCKET_ERROR_CODE;
    }

    return handle;
}

void close_socket(Socket::Handle handle) {
#ifdef _WIN32
    ::closesocket(handle);
#elif defined(__linux__)
    ::close(handle);
#endif
}

}

const int SocketOption::ReuseAddress = SO_REUSEADDR;
const int SocketOption::ReceiveBufferSize = SO_RCVBUF;
const int SocketOption::SendBufferSize = SO_SNDBUF;
const int SocketOption::ReceiveTimeout = SO_RCVTIMEO;
const int SocketOption::SendTimeout = SO_SNDTIMEO;

Socket::Socket() { 

}

Socket::Socket(Handle handle, Ip ip, Transport tp) {
    handle_ = handle;
    ip_ = ip;
    transport_ = tp;
}

Socket::~Socket() {
    close();
}

Socket::Socket(Socket&& other) noexcept
    : handle_(other.handle_)
{
    other.handle_ = -1;
}

Socket& Socket::operator=(Socket&& other) noexcept {
    handle_ = other.handle_;
    other.handle_ = -1;
    return *this;
}

void Socket::open(Ip ip, Transport tp, std::error_code &ec) {
    close();
    handle_ = detail::open_socket(ip, tp, ec);
}

void Socket::bind(const EndPoint& ep, std::error_code &ec) {
    auto address = ep.address();
    if (-1 == ::bind(
        handle_, 
        (const sockaddr*)address.addr_in_,
        address.is_v4() ? sizeof(sockaddr_in) : sizeof(sockaddr_in6)
    )) {
        ec = SOCKET_ERROR_CODE;
    }
}

void Socket::set_option(int op, SmallBytes bytes, std::error_code& ec) {
    int r = ::setsockopt(handle_, SOL_SOCKET, op, (const char*)bytes.data(), bytes.size());
    if (-1 == r) {
        ec = SOCKET_ERROR_CODE;
    }
}

SmallBytes Socket::get_option(int op, std::error_code &ec) {
    char buf[16];
    socklen_t len = sizeof(buf);
    int r = ::getsockopt(handle_, SOL_SOCKET, op, buf, &len);
    if (-1 == r) {
        ec = SOCKET_ERROR_CODE;
        return {};
    }

    return {buf};
}

Coro<> Socket::connect(const EndPoint& ep, std::error_code& ec) {
    IpAddress address = ep.address();

    this_context::get_service().relate((void*)handle_, ec);
    if (ec) {
        co_return;
    }

    magio::detail::ResumeHandle rhandle;
    IoContext ioc;
    ioc.handle = handle_;
    ioc.ptr = &rhandle;
    ioc.cb = magio::detail::completion_callback;

    std::memcpy(
        &ioc.remote_addr,
        address.addr_in_,
        address.is_v4() ? sizeof(sockaddr_in) : sizeof(sockaddr_in6)
    );

    co_await GetCoroutineHandle([&](std::coroutine_handle<> h) {
        rhandle.handle = h;
        this_context::get_service().connect(ioc);
    });

    ec = rhandle.ec;
}

Coro<size_t> Socket::read(char* buf, size_t len, std::error_code &ec) {
    magio::detail::ResumeHandle rhandle;
    IoContext ioc;
    ioc.handle = handle_;
    ioc.buf.buf = buf;
    ioc.buf.len = len;
    ioc.ptr = &rhandle;
    ioc.cb = magio::detail::completion_callback;

    co_await GetCoroutineHandle([&](std::coroutine_handle<> h) {
        rhandle.handle = h;
        this_context::get_service().receive(ioc);
    });

    ec = rhandle.ec;
    if (ec) {
        co_return {};
    }
    
    co_return ioc.buf.len;
}

Coro<size_t> Socket::write(const char* msg, size_t len, std::error_code &ec) {
    magio::detail::ResumeHandle rhandle;
    IoContext ioc;
    ioc.handle = handle_;
    ioc.buf.buf = (char*)msg;
    ioc.buf.len = len;
    ioc.ptr = &rhandle;
    ioc.cb = magio::detail::completion_callback;

    co_await GetCoroutineHandle([&](std::coroutine_handle<> h) {
        rhandle.handle = h;
        this_context::get_service().send(ioc);
    });

    ec = rhandle.ec;
    if (ec) {
        co_return {};
    }

    co_return ioc.buf.len;
}

void Socket::close() {
    if (handle_ != -1) {
        detail::close_socket(handle_);
        handle_ = -1;
    }
}

void Socket::shutdown(Shutdown type) {
    if (handle_ != - 1) {
        ::shutdown(handle_, (int)type);
    }
}

}

}