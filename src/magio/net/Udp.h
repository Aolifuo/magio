#pragma once

#include "magio/coro/Fwd.h"
#include "magio/net/Socket.h"

namespace magio {

class UdpSocket {
public:
    UdpSocket(Socket&& sock)
        : socket_(std::move(sock))
    { }

    static Coro<UdpSocket> bind(const char* host, uint_least16_t port);

    Coro<std::pair<size_t, SocketAddress>> read_from(char* buf, size_t len);
    Coro<size_t> write_to(const char* buf, size_t len, const SocketAddress&);

    AnyExecutor get_executor();
private:
    Socket socket_;
};


}