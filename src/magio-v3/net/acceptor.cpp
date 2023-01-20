#include "magio-v3/net/acceptor.h"

#include "magio-v3/core/error.h"
#include "magio-v3/core/coro_context.h"
#include "magio-v3/core/io_context.h"
#include "magio-v3/net/address.h"

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

Acceptor Acceptor::listen(const EndPoint &ep, std::error_code& ec) {
    Acceptor acceptor;
    auto& address = ep.address();
    acceptor.listener_ = Socket::open(address.ip(), Transport::Tcp, ec);
    if (ec) {
        return {};
    }

    acceptor.listener_.bind(ep, ec);
    if (ec) {
        return {};
    }

    // listen
    if (-1 == ::listen(acceptor.listener_.handle(), SOMAXCONN)) {
        ec = SYSTEM_ERROR_CODE;
        return {};
    }

    return acceptor;
}

#ifdef MAGIO_USE_CORO
Coro<std::pair<Socket, EndPoint>> Acceptor::accept(std::error_code& ec) {
    attach_context();
    struct AcceptResume: ResumeHandle {
        EndPoint ep;
    } rh;

    co_await GetCoroutineHandle([&](std::coroutine_handle<> h) {
        rh.handle = h;
        this_context::get_service().accept(listener_, &rh, 
            [](std::error_code ec, IoContext* ioc, void* ptr) {
                auto rh = (AcceptResume*)ptr;
                rh->ec = ec;
                rh->res = ioc->res;
                rh->ep = EndPoint(make_address((sockaddr*)&ioc->remote_addr), ::ntohs(ioc->remote_addr.sin_port));
                rh->handle.resume();

                delete ioc;
            });
    });

    ec = rh.ec;
    co_return {
        Socket((SocketHandle)rh.res, listener_.ip(), listener_.transport()),
        rh.ep
    };
}
#endif

void Acceptor::accept(std::function<void (std::error_code, Socket, EndPoint)> &&completion_cb) {
    using Cb = std::function<void (std::error_code, Socket, EndPoint)>;
    attach_context();

    this_context::get_service().accept(listener_, new Cb(std::move(completion_cb)), 
        [](std::error_code ec, IoContext* ioc, void* ptr) {
            auto cb = (Cb*)ptr;
            Ip ip = ioc->remote_addr.sin_family == AF_INET ? Ip::v4 : Ip::v6;
            (*cb)(
                ec, Socket((int)ioc->res, ip, Transport::Tcp),
                EndPoint(make_address((sockaddr*)&ioc->remote_addr), ::ntohs(ioc->remote_addr.sin_port))
            );
            delete cb;
            delete ioc;
        });
}

void Acceptor::attach_context() {
    listener_.attach_context();
}

}

}