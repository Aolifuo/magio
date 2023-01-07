#include "magio-v3/net/acceptor.h"

#include "magio-v3/core/coro_context.h"
#include "magio-v3/core/io_context.h"
#include "magio-v3/core/error.h"
#include "magio-v3/net/address.h"

#ifdef _WIN32
#include <Ws2tcpip.h>
#include <MSWSock.h>
#elif defined (__linux__)
#include <arpa/inet.h>
#endif

namespace magio {

namespace net {

Acceptor::Acceptor() { }

Acceptor::Acceptor(Acceptor&& other) noexcept
    : listener_(std::move(other.listener_)) 
{ }

Acceptor& Acceptor::operator=(Acceptor&& other) noexcept {
    listener_ = std::move(other.listener_);
    return *this;
}

void Acceptor::bind_and_listen(const EndPoint &ep, std::error_code& ec) {
    auto& address = ep.address();
    listener_.open(address.ip(), Transport::Tcp, ec);
    if (ec) {
        return;
    }

    listener_.bind(ep, ec);
    if (ec) {
        return;
    }

    // listen
    if (-1 == ::listen(listener_.handle(), SOMAXCONN)) {
        ec = SYSTEM_ERROR_CODE;
    }
}

void Acceptor::set_option(int op, SmallBytes bytes, std::error_code& ec) {
    listener_.set_option(op, bytes, ec);
}

SmallBytes Acceptor::get_option(int op, std::error_code &ec) {
    return listener_.get_option(op, ec);
}

#ifdef MAGIO_USE_CORO
Coro<std::pair<Socket, EndPoint>> Acceptor::accept(std::error_code& ec) {
    char buf[128];
    IoContext ioc;
    ResumeHandle handle;
    ioc.buf.buf = buf;
    ioc.buf.len = sizeof(buf);
    ioc.ptr = &handle;
    ioc.cb = completion_callback;

    co_await GetCoroutineHandle([&](std::coroutine_handle<> h) {
        handle.handle = h;
        this_context::get_service().accept(listener_, ioc);
    });
    
    ec = handle.ec;
    if (ec) {
        co_return {};
    }

#ifdef _WIN32
    net::Socket::Handle listener_h = listener_.handle();
    ::setsockopt(
        ioc.handle, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, 
        (const char*)&listener_h, sizeof(listener_h)
    );
#endif

    Ip ipv = ioc.remote_addr.sin_family == AF_INET ? Ip::v4 : Ip::v6;
    co_return {
        Socket(ioc.handle, ipv, Transport::Tcp), 
        EndPoint(
            make_address((sockaddr*)&ioc.remote_addr),
            ::ntohs(ioc.remote_addr.sin_port)
        )
    };
}
#endif

void Acceptor::accept(std::function<void (std::error_code, Socket, EndPoint)> &&completion_cb) {
    using Cb = std::function<void (std::error_code, Socket, EndPoint)>;
    auto ioc = new IoContext;
    ioc->buf.buf = new char[128];
    ioc->buf.len = 128;
    ioc->ptr = new Cb(std::move(completion_cb));
    ioc->cb = [](std::error_code ec, IoContext* ioc, void* ptr) {
        auto cb = (Cb*)ptr;
        if (ec) {
           (*cb)(ec, {}, {});
        } else {
#ifdef _WIN32
            auto listener_h = *(SOCKET*)&ioc->buf.buf[120];
            ::setsockopt(
                ioc->handle, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, 
                (const char*)&listener_h, sizeof(listener_h)
            );
#endif
            Ip ipv = ioc->remote_addr.sin_family == AF_INET ? Ip::v4 : Ip::v6;
            (*cb)(
                ec, 
                Socket(ioc->handle, ipv, Transport::Tcp), 
                EndPoint(
                    make_address((sockaddr*)&ioc->remote_addr),
                    ::ntohs(ioc->remote_addr.sin_port)
                )
            );
        }

        delete ioc->buf.buf;
        delete cb;
        delete ioc;
    };

    auto listener_h = listener_.handle();
    std::memcpy(&ioc->buf.buf[120], &listener_h, sizeof(listener_h));
    this_context::get_service().accept(listener_, *ioc);
}

}

}