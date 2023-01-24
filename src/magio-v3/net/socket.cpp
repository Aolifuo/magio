#include "magio-v3/net/socket.h"

#include "magio-v3/core/logger.h"
#include "magio-v3/core/io_context.h"
#include "magio-v3/core/coro_context.h"
#include "magio-v3/net/address.h"

#ifdef _WIN32

#elif defined(__linux__)
#include <unistd.h>
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
    reset();
}

Socket::Socket(Handle handle, Ip ip, Transport tp) {
    handle_ = handle;
    attached_ = nullptr;
    ip_ = ip;
    transport_ = tp;
}

Socket::~Socket() {
    close();
}

Socket::Socket(Socket&& other) noexcept
    : handle_(other.handle_)
    , attached_(other.attached_)
    , ip_(other.ip_)
    , transport_(other.transport_)
{
    other.reset();
}

Socket& Socket::operator=(Socket&& other) noexcept {
    if (handle_ == other.handle_) { // self
        return *this;
    }
    
    handle_ = other.handle_;
    attached_ = other.attached_;
    ip_ = other.ip_;
    transport_ = other.transport_;
    other.reset();
    return *this;
}

Result<Socket> Socket::open(Ip ip, Transport tp) {
    std::error_code ec;
    Socket socket(detail::open_socket(ip, tp, ec), ip, tp);
    return {std::move(socket), ec};
}

Result<> Socket::bind(const EndPoint& ep) {
    std::error_code ec;
    auto& address = ep.address();
    if (-1 == ::bind(
        handle_, 
        (const sockaddr*)address.addr_in_,
        address.addr_len()
    )) {
        return {SYSTEM_ERROR_CODE};
    }

    return {};
}

#ifdef MAGIO_USE_CORO
Coro<Result<>> Socket::connect(const EndPoint& ep) {
    attach_context();
    std::error_code ec;
    ResumeHandle rh;
    co_await GetCoroutineHandle([&](std::coroutine_handle<> h) {
        rh.handle = h;
        this_context::get_service().connect(handle_, ep, &rh, resume_callback);
    });

    co_return {rh.ec};
}

Coro<Result<size_t>> Socket::receive(char* buf, size_t len) {
    attach_context();
    std::error_code ec;
    ResumeHandle rh;
    co_await GetCoroutineHandle([&](std::coroutine_handle<> h) {
        rh.handle = h;
        this_context::get_service().receive(handle_, buf, len, &rh, resume_callback);
    });

    co_return {rh.res, rh.ec};
}

Coro<Result<size_t>> Socket::send(const char* msg, size_t len) {
    attach_context();
    ResumeHandle rh;

    co_await GetCoroutineHandle([&](std::coroutine_handle<> h) {
        rh.handle = h;
        this_context::get_service().send(handle_, msg, len, &rh, resume_callback);
    });

    co_return {rh.res, rh.ec};
}

Coro<Result<size_t>> Socket::send_to(const char* msg, size_t len, const EndPoint& ep) {
    attach_context();
    ResumeHandle rh;

    co_await GetCoroutineHandle([&](std::coroutine_handle<> h) {
        rh.handle = h;
        this_context::get_service().send_to(handle_, ep, msg, len, &rh, resume_callback);
    });

    co_return {rh.res, rh.ec};
}

Coro<Result<std::pair<size_t, EndPoint>>> Socket::receive_from(char* buf, size_t len) {
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
                rh->ep = EndPoint(_make_address((sockaddr*)&ioc->remote_addr), ::ntohs(ioc->remote_addr.sin_port));
                rh->handle.resume();

                delete ioc;
            });
    });

    if (rh.ec) {
        co_return {rh.ec};
    }
    co_return {{rh.res, std::move(rh.ep)}};
}
#endif

void Socket::connect(const EndPoint &ep, Functor<void (std::error_code)> &&completion_cb) {
    using Cb = Functor<void (std::error_code)>;
    attach_context();

    this_context::get_service().connect(handle_, ep, new Cb(std::move(completion_cb)), 
        [](std::error_code ec, IoContext* ioc, void* ptr) {
            auto cb = (Cb*)ptr;
            (*cb)(ec);
            delete cb;
            delete ioc;
        });
}

void Socket::receive(char *buf, size_t len, Functor<void (std::error_code, size_t)> &&completion_cb) {
    using Cb = Functor<void (std::error_code, size_t)>;
    attach_context();

    this_context::get_service().receive(handle_, buf, len, new Cb(std::move(completion_cb)),
        [](std::error_code ec, IoContext* ioc, void* ptr) {
            auto cb = (Cb*)ptr;
            (*cb)(ec, ioc->res);
            delete cb;
            delete ioc;
        });
}

void Socket::send(const char *msg, size_t len, Functor<void (std::error_code, size_t)> &&completion_cb) {
    using Cb = Functor<void (std::error_code, size_t)>;
    attach_context();

    this_context::get_service().send(handle_, msg, len, new Cb(std::move(completion_cb)),
        [](std::error_code ec, IoContext* ioc, void* ptr) {
            auto cb = (Cb*)ptr;
            (*cb)(ec, ioc->res);
            delete cb;
            delete ioc;
        });
}

void Socket::send_to(const char *msg, size_t len, const EndPoint &ep, Functor<void (std::error_code, size_t)> &&completion_cb) {
    using Cb = Functor<void (std::error_code, size_t)>;
    attach_context();

    this_context::get_service().send_to(handle_, ep, msg, len, new Cb(std::move(completion_cb)), 
        [](std::error_code ec, IoContext* ioc, void* ptr) {
            auto cb = (Cb*)ptr;
            (*cb)(ec, ioc->res);
            delete cb;
            delete ioc;
        });
}

void Socket::receive_from(char *buf, size_t len, Functor<void (std::error_code, size_t, EndPoint)>&& completion_cb) {
    using Cb = Functor<void (std::error_code, size_t, EndPoint)>;
    attach_context();

    this_context::get_service().receive_from(handle_, buf, len, new Cb(std::move(completion_cb)),
        [](std::error_code ec, IoContext* ioc, void* ptr) {
            auto cb = (Cb*)ptr;
            if (ec) {
                (*cb)(ec, {}, {});
            } else {
                (*cb)(
                    ec, ioc->res,
                    EndPoint(_make_address((sockaddr*)&ioc->remote_addr), ::ntohs(ioc->remote_addr.sin_port))
                );
            }
            delete cb;
            delete ioc;
        });
}

void Socket::cancel() {
    if (-1 != handle_) {
        this_context::get_service().cancel(IoHandle{.a = handle_});
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
    if (-1 == handle_) {
        return;
    }

    if (!attached_) {
        std::error_code ec;
        attached_ = LocalContext;
        this_context::get_service().attach(IoHandle{.a = handle_}, ec);
    } else if (attached_ != LocalContext) {
        M_FATAL("{}", "The socket cannot be attached to different context");
    }
}

void Socket::reset() {
    handle_ = -1;
    attached_ = nullptr;
    ip_ = Ip::v4;
    transport_ = Transport::Tcp;
}

}

}