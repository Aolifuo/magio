#ifndef MAGIO_NET_ACCEPTOR_H_
#define MAGIO_NET_ACCEPTOR_H_

#include "magio-v3/utils/noncopyable.h"
#include "magio-v3/core/error.h"
#include "magio-v3/net/socket.h"

namespace magio {

namespace net {

class Acceptor: Noncopyable {
public:
    Acceptor();

    Acceptor(Acceptor&& other) noexcept;

    Acceptor& operator=(Acceptor&& other) noexcept;

    [[nodiscard]]
    static Result<Acceptor> listen(const InetAddress& address);

#ifdef MAGIO_USE_CORO
    [[nodiscard]]
    Coro<Result<std::pair<Socket, InetAddress>>> accept();
#endif

    void accept(Functor<void(std::error_code, Socket, InetAddress)>&& completion_cb);

    void attach_context();

private:
    Socket listener_;
};

}

}

#endif