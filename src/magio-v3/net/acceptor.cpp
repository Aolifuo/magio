#include "magio-v3/net/acceptor.h"

#include "magio-v3/core/coro_context.h"
#include "magio-v3/core/io_context.h"
#include "magio-v3/net/address.h"
#include "magio-v3/core/detail/completion_callback.h"
#include <arpa/inet.h>

#ifdef _WIN32
#include <Ws2tcpip.h>
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

void Acceptor::bind_and_listen(const EndPoint &ep, Transport tp, std::error_code& ec) {
    IpAddress address = ep.address();
    listener_.open(address.ip(), tp, ec);
    if (ec) {
        return;
    }

    listener_.bind(ep, ec);
    if (ec) {
        return;
    }

    this_context::get_service().relate((void*)listener_.handle(), ec);
    if (ec) {
        return;
    }

    // listen
    if (-1 == ::listen(listener_.handle(), SOMAXCONN)) {
        ec = SOCKET_ERROR_CODE;
    }
}

void Acceptor::set_option(int op, SmallBytes bytes, std::error_code& ec) {
    listener_.set_option(op, bytes, ec);
}

SmallBytes Acceptor::get_option(int op, std::error_code &ec) {
    return listener_.get_option(op, ec);
}

Coro<std::pair<Socket, EndPoint>> Acceptor::accept(std::error_code& ec) {
    char buf[128];
    IoContext ioc;
    magio::detail::ResumeHandle handle;
    ioc.buf.buf = buf;
    ioc.buf.len = sizeof(buf);
    ioc.ptr = &handle;
    ioc.cb = magio::detail::completion_callback;

    co_await GetCoroutineHandle([&](std::coroutine_handle<> h) {
        handle.handle = h;
        this_context::get_service().accept(listener_, ioc);
    });
    
    ec = handle.ec;
    if (ec) {
        detail::close_socket(ioc.handle);
        co_return {};
    }

    this_context::get_service().relate((void*)ioc.handle, ec);
    if (ec) {
        detail::close_socket(ioc.handle);
        co_return {};
    }

    EndPoint ep;
    Ip ipv;
    Transport trans = listener_.transport();

    std::memset(buf, 0, sizeof(buf));
    ::inet_ntop(ioc.remote_addr.sin_family, &ioc.remote_addr, buf, sizeof(buf));

    if (ioc.remote_addr.sin_family == AF_INET) {
        ipv = Ip::v4;
        ep = EndPoint(
            IpAddress(ioc.remote_addr, buf, Ip::v4),
            ::ntohs(ioc.remote_addr.sin_port)
        );
    } else {
        ipv = Ip::v6;
        ep = EndPoint(
            IpAddress(ioc.remote_addr6, buf, Ip::v6),
            ::ntohs(ioc.remote_addr6.sin6_port)
        );
    }

    co_return {
        Socket(ioc.handle, ipv, trans), 
        ep
    };
}

}

}