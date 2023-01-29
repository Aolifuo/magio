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

Result<Acceptor> Acceptor::listen(const InetAddress &address) {
    std::error_code ec;
    Acceptor acceptor;
    acceptor.listener_ = Socket::open(address.is_ipv4() ? Ip::v4 : Ip::v6, Transport::Tcp) | redirect_err(ec);
    if (ec) {
        return {ec};
    }

    acceptor.listener_.bind(address) | redirect_err(ec);
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
Coro<Result<std::pair<Socket, InetAddress>>> Acceptor::accept() {
    attach_context();
    struct AcceptResume: ResumeHandle {
        InetAddress address;
    } rh;

    co_await GetCoroutineHandle([&](std::coroutine_handle<> h) {
        rh.handle = h;
        this_context::get_service().accept(listener_, &rh, 
            [](std::error_code ec, IoContext* ioc, void* ptr) {
                auto rh = (AcceptResume*)ptr;
                rh->ec = ec;
                rh->res = ioc->res;
                rh->address = InetAddress::from((sockaddr*)&ioc->remote_addr);
                rh->handle.resume();

                delete ioc;
            });
    });

    if (rh.ec) {
        co_return {rh.ec};
    }
    co_return {{Socket((SocketHandle)rh.res, listener_.ip(), listener_.transport()), std::move(rh.address)}};
}
#endif

void Acceptor::accept(Functor<void (std::error_code, Socket, InetAddress)> &&completion_cb) {
    using Cb = Functor<void (std::error_code, Socket, InetAddress)>;
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
                    InetAddress::from((sockaddr*)&ioc->remote_addr)
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