#include "magio/net/Udp.h"

#include "magio/dev/Log.h"
#include "magio/coro/Coro.h"
#include "magio/plat/io_service.h"

namespace magio {

struct UdpHook {
    TimerID             id = MAGIO_INVALID_TIMERID;
    plat::socket_type   fd;
    Waker               w;
    std::error_code     ec;
};

void resume_from_hook2(std::error_code ec, void* ptr, plat::socket_type fd) {
    auto phook = (UdpHook*)ptr;
    phook->fd = fd;
    phook->ec = ec;
    if (phook->id != MAGIO_INVALID_TIMERID) {
        if(phook->w.node_->executor_.cancel(phook->id)) {
            phook->id = MAGIO_INVALID_TIMERID;
        }
    }

    phook->w.wake();
}

// UdpSocket

Coro<UdpSocket> UdpSocket::bind(const char* host, uint_least16_t port) {
    auto exe = co_await this_coro::executor;
    
    auto socket = Socket::create(exe, Protocol::UDP).expect();
    auto address = make_address(host, port).expect();
    socket.bind(address).expect();

#ifdef _WIN32
    socket.get_executor().get_service().relate(socket.handle());
#endif

    co_return {std::move(socket)};
}

Coro<std::pair<size_t, SocketAddress>> UdpSocket::read_from(char* buf, size_t len) {
    auto hook = UdpHook{};

    plat::IOData io;
    io.fd = socket_.handle();
    io.wsa_buf.buf = buf;
    io.wsa_buf.len = len;
    io.ptr = &hook;
    io.cb = resume_from_hook2;

    co_await Awaitable {
        [&hook, pio = &io, this](AnyExecutor exe, Waker waker, size_t tm) {
            hook.w = waker;
            socket_.get_executor().get_service().async_receive_from(pio);
            
            // cancel io
            if (tm != MAGIO_MAX_TIME) {
                hook.id = exe.invoke_after([sock = pio->fd](bool exit) {
                    if (exit) {
                        return;
                    }

                    cancel_io(sock);
                }, tm);
            }
        }
    };

    if (hook.id != MAGIO_INVALID_TIMERID) {
        co_await detail::WakeAfterTimeout{};
    }

    if (hook.ec) {
        throw std::system_error(hook.ec);
    }
#ifdef __linux__
    co_return {
        io.wsa_buf.len,
        plat::remote_address(io.fd)};
#endif
#ifdef _WIN32
    char addr_buf[40]{};
    ::inet_ntop(
        io.remote.sin_family,
        &io.remote.sin_addr,  
        addr_buf, 40);

    co_return {io.wsa_buf.len, {addr_buf, ::ntohs(io.remote.sin_port), io.remote}};
#endif
}

Coro<size_t> UdpSocket::write_to(const char* buf, size_t len, const SocketAddress& address) {
    auto hook = UdpHook{};

    plat::IOData io;

    if (0 == address._sockaddr.sin_port) {
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(address.host);

        if (-1 == ::inet_pton(AF_INET, address.ip.c_str(), &addr.sin_addr)) {
            MAGIO_THROW_SYSTEM_ERROR;
        }
        
        io.remote = addr;   
    } else {
        io.remote = address._sockaddr;
    }

    io.fd = socket_.handle();
    io.wsa_buf.buf = (char*)buf;
    io.wsa_buf.len = len;
    io.ptr = &hook;
    io.cb = resume_from_hook2;

    co_await Awaitable {
        [&hook, pio = &io, this](AnyExecutor exe, Waker waker, size_t tm) {
            hook.w = waker;
            socket_.get_executor().get_service().async_send_to(pio);

            // cancel io
            if (tm != MAGIO_MAX_TIME) {
                hook.id = exe.invoke_after([sock = pio->fd](bool exit) {
                    if (exit) {
                        return;
                    }

                    cancel_io(sock);
                }, tm);
            }
        }
    };

    if (hook.id != MAGIO_INVALID_TIMERID) {
        co_await detail::WakeAfterTimeout{};
    }

    if (hook.ec) {
        throw std::system_error(hook.ec);
    }

    co_return io.wsa_buf.len;
}

AnyExecutor UdpSocket::get_executor() {
    return socket_.get_executor();
}

}