#include "magio-v3/net/acceptor.h"

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
    if (listener_.handle() == other.listener_.handle()) { // self
        return *this;
    }

    listener_ = std::move(other.listener_);
    return *this;
}

Result<Acceptor> Acceptor::listen(const EndPoint &ep) {
    std::error_code ec;
    Acceptor acceptor;
    acceptor.listener_ = Socket::open(ep.address().ip(), Transport::Tcp) | get_err(ec);
    if (ec) {
        return {ec};
    }

    acceptor.listener_.bind(ep) | get_err(ec);
    if (ec) {
        return {ec};
    }

    // listen
    if (-1 == ::listen(acceptor.listener_.handle(), SOMAXCONN)) {
        return {SYSTEM_ERROR_CODE};
    }

    return {std::move(acceptor)};
}

#ifdef MAGIO_USE_CORO
Coro<Result<std::pair<Socket, EndPoint>>> Acceptor::accept() {
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
                rh->ep = EndPoint(_make_address((sockaddr*)&ioc->remote_addr), ::ntohs(ioc->remote_addr.sin_port));
                rh->handle.resume();

                delete ioc;
            });
    });

    if (rh.ec) {
        co_return {rh.ec};
    }
    co_return {{Socket((SocketHandle)rh.res, listener_.ip(), listener_.transport()), std::move(rh.ep)}};
}
#endif

void Acceptor::accept(Functor<void (std::error_code, Socket, EndPoint)> &&completion_cb) {
    using Cb = Functor<void (std::error_code, Socket, EndPoint)>;
    attach_context();

    this_context::get_service().accept(listener_, new Cb(std::move(completion_cb)), 
        [](std::error_code ec, IoContext* ioc, void* ptr) {
            auto cb = (Cb*)ptr;
            Ip ip = ioc->remote_addr.sin_family == AF_INET ? Ip::v4 : Ip::v6;
            if (ec) {
                (*cb)(ec, {}, {});
            } else {
                (*cb)(
                    ec, Socket((int)ioc->res, ip, Transport::Tcp),
                    EndPoint(_make_address((sockaddr*)&ioc->remote_addr), ::ntohs(ioc->remote_addr.sin_port))
                );
            }
            delete cb;
            delete ioc;
        });
}

void Acceptor::attach_context() {
    listener_.attach_context();
}

}

}