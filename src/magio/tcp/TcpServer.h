#pragma once

#include "magio/core/Pimpl.h"
#include "magio/coro/UseCoro.h"

namespace magio {

class TcpStream;

class TcpServer {

    CLASS_PIMPL_DECLARE(TcpServer)

public:
    static Coro<TcpServer> bind(const char* host, short port);
    Coro<TcpStream> accept();
};

class TcpStream {
    friend TcpServer;

    CLASS_PIMPL_DECLARE(TcpStream)

public:

    Coro<size_t> read(char* buf, size_t len);
    Coro<size_t> write(const char* data, size_t len);
};

}