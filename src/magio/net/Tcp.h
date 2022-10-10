#pragma once

#include "magio/coro/Fwd.h"
#include "magio/core/Pimpl.h"
#include "magio/net/SocketAddress.h"

namespace magio {

class TcpStream {
    friend class TcpServer;
    friend class TcpClient;

    CLASS_PIMPL_DECLARE(TcpStream)

public:
    SocketAddress local_address();
    SocketAddress remote_address();

    static Coro<TcpStream> connect(const char* host, uint_least16_t port);

    Coro<size_t> read(char* buf, size_t len);
    Coro<size_t> write(const char* buf, size_t len);
};

class TcpServer {

    CLASS_PIMPL_DECLARE(TcpServer)

public:
    static Coro<TcpServer> bind(const char* host, uint_least16_t port);
    Coro<TcpStream> accept();
};


}