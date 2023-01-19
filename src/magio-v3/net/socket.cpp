#include "magio-v3/net/socket.h"


#include "magio-v3/core/error.h"
#include "magio-v3/core/io_context.h"
#include "magio-v3/core/coro_context.h"
#include "magio-v3/net/address.h"

#ifdef _WIN32

#elif defined(__linux__)
#include <unistd.h>
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
        ec = SYSTEM_ERROR_CODE;
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

Socket::Socket() { 

}

Socket::Socket(Handle handle, Ip ip, Transport tp) {
    handle_ = handle;
    is_attached_ = false;
    ip_ = ip;
    transport_ = tp;
}

Socket::~Socket() {
    close();
}

Socket::Socket(Socket&& other) noexcept
    : handle_(other.handle_)
    , is_attached_(other.is_attached_)
    , ip_(other.ip_)
    , transport_(other.transport_)
{
    other.reset();
}

Socket& Socket::operator=(Socket&& other) noexcept {
    handle_ = other.handle_;
    is_attached_ = other.is_attached_;
    ip_ = other.ip_;
    transport_ = other.transport_;
    other.reset();
    return *this;
}

Socket Socket::open(Ip ip, Transport tp, std::error_code &ec) {
    Socket socket;
    socket.handle_ = detail::open_socket(ip, tp, ec);
    socket.ip_ = ip;
    socket.transport_ = tp;
    return socket;
}

void Socket::bind(const EndPoint& ep, std::error_code &ec) {
    auto& address = ep.address();
    if (-1 == ::bind(
        handle_, 
        (const sockaddr*)address.addr_in_,
        address.addr_len()
    )) {
        ec = SYSTEM_ERROR_CODE;
        return;
    }
}

#ifdef MAGIO_USE_CORO
Coro<> Socket::connect(const EndPoint& ep, std::error_code& ec) {
    attach_context();

    ResumeHandle rh;
    co_await GetCoroutineHandle([&](std::coroutine_handle<> h) {
        rh.handle = h;
        this_context::get_service().connect(handle_, ep, &rh, resume_callback);
    });

    ec = rh.ec;
}

Coro<size_t> Socket::receive(char* buf, size_t len, std::error_code &ec) {
    attach_context();

    ResumeHandle rh;
    co_await GetCoroutineHandle([&](std::coroutine_handle<> h) {
        rh.handle = h;
        this_context::get_service().receive(handle_, buf, len, &rh, resume_callback);
    });

    ec = rh.ec;
    co_return rh.res;
}

Coro<size_t> Socket::send(const char* msg, size_t len, std::error_code &ec) {
    attach_context();
    ResumeHandle rh;

    co_await GetCoroutineHandle([&](std::coroutine_handle<> h) {
        rh.handle = h;
        this_context::get_service().send(handle_, msg, len, &rh, resume_callback);
    });

    ec = rh.ec;
    co_return rh.res;
}

Coro<size_t> Socket::send_to(const char* msg, size_t len, const EndPoint& ep, std::error_code& ec) {
    attach_context();
    ResumeHandle rh;

    co_await GetCoroutineHandle([&](std::coroutine_handle<> h) {
        rh.handle = h;
        this_context::get_service().send_to(handle_, ep, msg, len, &rh, resume_callback);
    });

    ec = rh.ec;
    co_return rh.res;
}

Coro<std::pair<size_t, EndPoint>> Socket::receive_from(char* buf, size_t len, std::error_code& ec) {
    attach_context();
    struct RecvResume: ResumeHandle {
        EndPoint ep;
    } rh;

    co_await GetCoroutineHandle([&](std::coroutine_handle<> h) {
        rh.handle = h;
        this_context::get_service().receive_from(handle_, buf, len, &rh, 
            [](std::error_code ec, IoContext* ioc, void* ptr) {
                auto rh = (RecvResume*)ptr;
                rh->ec = ec;
                rh->res = ioc->res;
                rh->ep = EndPoint(make_address((sockaddr*)&ioc->remote_addr), ::ntohs(ioc->remote_addr.sin_port));
                rh->handle.resume();

                delete ioc;
            });
    });

    ec = rh.ec;
    co_return {rh.res, rh.ep};
}
#endif

void Socket::connect(const EndPoint &ep, std::function<void (std::error_code)> &&completion_cb) {
    using Cb = std::function<void (std::error_code)>;
    attach_context();

    this_context::get_service().connect(handle_, ep, new Cb(std::move(completion_cb)), 
        [](std::error_code ec, IoContext* ioc, void* ptr) {
            auto cb = (Cb*)ptr;
            (*cb)(ec);
            delete cb;
            delete ioc;
        });
}

void Socket::receive(char *buf, size_t len, std::function<void (std::error_code, size_t)> &&completion_cb) {
    using Cb = std::function<void (std::error_code, size_t)>;
    attach_context();

    this_context::get_service().receive(handle_, buf, len, new Cb(std::move(completion_cb)),
        [](std::error_code ec, IoContext* ioc, void* ptr) {
            auto cb = (Cb*)ptr;
            (*cb)(ec, ioc->res);
            delete cb;
            delete ioc;
        });
}

void Socket::send(const char *msg, size_t len, std::function<void (std::error_code, size_t)> &&completion_cb) {
    using Cb = std::function<void (std::error_code, size_t)>;
    attach_context();

    this_context::get_service().send(handle_, msg, len, new Cb(std::move(completion_cb)),
        [](std::error_code ec, IoContext* ioc, void* ptr) {
            auto cb = (Cb*)ptr;
            (*cb)(ec, ioc->res);
            delete cb;
            delete ioc;
        });
}

void Socket::send_to(const char *msg, size_t len, const EndPoint &ep, std::function<void (std::error_code, size_t)> &&completion_cb) {
    using Cb = std::function<void (std::error_code, size_t)>;
    attach_context();

    this_context::get_service().send_to(handle_, ep, msg, len, new Cb(std::move(completion_cb)), 
        [](std::error_code ec, IoContext* ioc, void* ptr) {
            auto cb = (Cb*)ptr;
            (*cb)(ec, ioc->res);
            delete cb;
            delete ioc;
        });
}

void Socket::receive_from(char *buf, size_t len, std::function<void (std::error_code, size_t, EndPoint)>&& completion_cb) {
    using Cb = std::function<void (std::error_code, size_t, EndPoint)>;
    attach_context();

    this_context::get_service().receive_from(handle_, buf, len, new Cb(std::move(completion_cb)),
        [](std::error_code ec, IoContext* ioc, void* ptr) {
            auto cb = (Cb*)ptr;
            (*cb)(
                ec, ioc->res,
                EndPoint(make_address((sockaddr*)&ioc->remote_addr), ::ntohs(ioc->remote_addr.sin_port))
            );
            delete cb;
            delete ioc;
        });
}

void Socket::cancel() {
    if (-1 != handle_) {
        // 
    }
}

void Socket::close() {
    if (-1 != handle_) {
        detail::close_socket(handle_);
        reset();
    }
}

void Socket::shutdown(Shutdown type) {
    if (handle_ != - 1) {
        ::shutdown(handle_, (int)type);
    }
}

void Socket::attach_context() {
    if (-1 != handle_ && !is_attached_) {
        std::error_code ec;
        is_attached_ = true;
        this_context::get_service().attach(IoHandle{.b = handle_}, ec);
    }
}

void Socket::reset() {
    handle_ = -1;
    is_attached_ = false;
    ip_ = Ip::v4;
    transport_ = Transport::Tcp;
}

}

}