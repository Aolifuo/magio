#ifndef MAGIO_NET_ACCEPTOR_H_
#define MAGIO_NET_ACCEPTOR_H_

#include "magio-v3/core/noncopyable.h"
#include "magio-v3/net/socket.h"

namespace magio {

namespace net {

class Acceptor: Noncopyable {
public:
    Acceptor();

    Acceptor(Acceptor&& other) noexcept;

    Acceptor& operator=(Acceptor&& other) noexcept;

    [[nodiscard]]
    static Acceptor bind_and_listen(const EndPoint& ep, std::error_code& ec);

    void set_option(int op, SmallBytes bytes, std::error_code& ec);

    SmallBytes get_option(int op, std::error_code& ec);

#ifdef MAGIO_USE_CORO
    [[nodiscard]]
    Coro<std::pair<Socket, EndPoint>> accept(std::error_code& ec);

#endif
    void accept(std::function<void(std::error_code, Socket, EndPoint)>&& completion_cb);

private:
    Socket listener_;
};

}

}

#endif