#pragma once

#include "magio/coro/Fwd.h"
#include "magio/core/Pimpl.h"
#include "magio/net/SocketAddress.h"

namespace magio {

class UdpSocket {
    friend class UdpServer;

    CLASS_PIMPL_DECLARE(UdpSocket)

public:
    static Coro<UdpSocket> bind(const char* host, uint_least16_t port);

    Coro<std::pair<size_t, SocketAddress>> read_from(char* buf, size_t len);
    Coro<size_t> write_to(const char* buf, size_t len, const SocketAddress&);
};


}