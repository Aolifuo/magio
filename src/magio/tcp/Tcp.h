#pragma once

#include "magio/plat/declare.h"
#include "magio/core/Pimpl.h"
#include "magio/coro/UseCoro.h"

namespace magio {

class TcpStream {
    friend class TcpServer;
    friend class TcpClient;

    CLASS_PIMPL_DECLARE(TcpStream)

public:
    Address local_address();
    Address remote_address();

    Coro<size_t> read(char* buf, size_t len);
    Coro<size_t> write(const char* data, size_t len);
};

class TcpServer {

    CLASS_PIMPL_DECLARE(TcpServer)

public:
    static Coro<TcpServer> bind(const char* host, short port);
    Coro<TcpStream> accept();
};

class TcpClient {

    CLASS_PIMPL_DECLARE(TcpClient)

public:
    static Coro<TcpClient> create();

    Coro<TcpStream> connect(const char* host1, short port1, const char *host2, short port2);
};

}