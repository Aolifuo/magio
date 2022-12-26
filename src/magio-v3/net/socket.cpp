#include "magio-v3/net/socket.h"

#include "magio-v3/core/error.h"
#include "magio-v3/core/coro_context.h"
#include "magio-v3/core/io_context.h"

#ifdef _WIN32
#include <Ws2tcpip.h>
#elif defined(__linux__)
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
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
            af, SOCK_DGRAM, IPPROTO_UDP,
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
    : is_related_(other.is_related_)
    , handle_(other.handle_)
    , ip_(other.ip_)
    , transport_(other.transport_)
{
    other.reset();
}

Socket& Socket::operator=(Socket&& other) noexcept {
    is_related_ = other.is_related_;
    handle_ = other.handle_;
    ip_ = other.ip_;
    transport_ = other.transport_;
    other.reset();
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
        return;
    }

    this_context::get_service().relate((void*)handle_, ec);
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
    check_relation();
    auto& address = ep.address();

    ResumeHandle rhandle;
    IoContext ioc;
    ioc.handle = handle_;
    ioc.ptr = &rhandle;
    ioc.cb = completion_callback;

    ioc.addr_len = address.is_v4() ? sizeof(sockaddr_in) : sizeof(sockaddr_in6);
    std::memcpy(&ioc.remote_addr, address.addr_in_, ioc.addr_len);

    co_await GetCoroutineHandle([&](std::coroutine_handle<> h) {
        rhandle.handle = h;
        this_context::get_service().connect(ioc);
    });

    ec = rhandle.ec;
}

Coro<size_t> Socket::read(char* buf, size_t len, std::error_code &ec) {
    check_relation();
    ResumeHandle rhandle;
    IoContext ioc;
    ioc.handle = handle_;
    ioc.buf.buf = buf;
    ioc.buf.len = len;
    ioc.ptr = &rhandle;
    ioc.cb = completion_callback;

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
    check_relation();
    ResumeHandle rhandle;
    IoContext ioc;
    ioc.handle = handle_;
    ioc.buf.buf = (char*)msg;
    ioc.buf.len = len;
    ioc.ptr = &rhandle;
    ioc.cb = completion_callback;

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

Coro<size_t> Socket::write_to(const char* msg, size_t len, const EndPoint& ep, std::error_code& ec) {
    check_relation();
    IoContext ioc;

    ioc.handle = handle_;
    ioc.buf.buf = (char*)msg;
    ioc.buf.len = len;
    ioc.addr_len = ep.address().is_v4() ? sizeof(sockaddr_in) : sizeof(sockaddr_in6);
    std::memcpy(&ioc.remote_addr, ep.address().addr_in_, ioc.addr_len);
#ifdef _WIN32
    ResumeHandle rhandle;
    ioc.cb = completion_callback;
#elif defined (__linux__)
    ResumeWithMsg rhandle;
    rhandle.msg.msg_name = &ioc.remote_addr;
    rhandle.msg.msg_namelen = ioc.addr_len;
    rhandle.msg.msg_iov = (iovec*)&ioc.buf;
    rhandle.msg.msg_iovlen = 1;
    rhandle.msg.msg_control = nullptr;
    rhandle.msg.msg_controllen = 0;
    rhandle.msg.msg_flags = 0;
    ioc.cb = completion_callback_with_msg;
#endif
    ioc.ptr = &rhandle;

    co_await GetCoroutineHandle([&](std::coroutine_handle<> h) {
        rhandle.handle = h;
        this_context::get_service().send_to(ioc);
    });

    ec = rhandle.ec;
    if (ec) {
        co_return {};
    }

    co_return ioc.buf.len;
}

Coro<std::pair<size_t, EndPoint>> Socket::receive_from(char* msg, size_t len, std::error_code& ec) {
    check_relation();
    char buf[32];
    IpAddress address;
    EndPoint peer;
    IoContext ioc;

    ioc.handle = handle_;
    ioc.buf.buf = msg;
    ioc.buf.len = len;
#ifdef _WIN32
    ResumeHandle rhandle;
    ioc.addr_len = sizeof(sockaddr_in6);
    ioc.cb = completion_callback;
#elif defined (__linux__)
    ResumeWithMsg rhandle;
    rhandle.msg.msg_name = &ioc.remote_addr;
    rhandle.msg.msg_namelen = sizeof(sockaddr_in6);
    rhandle.msg.msg_iov = (iovec*)&ioc.buf;
    rhandle.msg.msg_iovlen = 1;
    rhandle.msg.msg_control = nullptr;
    rhandle.msg.msg_controllen = 0;
    rhandle.msg.msg_flags = 0;
    ioc.cb = completion_callback_with_msg;
#endif
    ioc.ptr = &rhandle;
    
    co_await GetCoroutineHandle([&](std::coroutine_handle<> h) {
        rhandle.handle = h;
        this_context::get_service().receive_from(ioc);
    });

    ec = rhandle.ec;
    if (ec) {
        co_return {};
    }

    ioc.addr_len = ioc.remote_addr.sin_family
        == AF_INET ? sizeof(sockaddr_in) : sizeof(sockaddr_in6);

    ::inet_ntop(
        ioc.remote_addr.sin_family, &ioc.remote_addr, 
        buf, sizeof(buf)
    );
    std::memcpy(address.addr_in_, &ioc.remote_addr, ioc.addr_len);

    address.ip_ = buf;
    if (ioc.remote_addr.sin_family == AF_INET) {
        address.level_ = Ip::v4;
        peer = EndPoint(
            address, ::ntohs(ioc.remote_addr.sin_port)
        );
    } else {
        address.level_ = Ip::v6;
        peer = EndPoint(
            address, ::ntohs(ioc.remote_addr6.sin6_port)
        );
    }

    co_return {
        ioc.buf.len, 
        peer
    };
}

void Socket::cancel() {

}

void Socket::close() {
    if (handle_ != -1) {
        detail::close_socket(handle_);
        reset();
    }
}

void Socket::shutdown(Shutdown type) {
    if (handle_ != - 1) {
        ::shutdown(handle_, (int)type);
    }
}

void Socket::reset() {
    is_related_ = false;
    handle_ = -1;
    ip_ = Ip::v4;
    transport_ = Transport::Tcp;
}

void Socket::check_relation() {
    if (handle_ != -1 && !is_related_) {
        std::error_code ec;
        this_context::get_service().relate((void*)handle_, ec);
        is_related_ = true;
    }
}

}

}