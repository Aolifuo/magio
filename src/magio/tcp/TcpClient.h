#pragma once

#include "magio/core/Execution.h"
#include "magio/core/Pimpl.h"
#include "magio/coro/UseCoro.h"

namespace magio {

class TcpConnection;

class TcpClient {

    CLASS_PIMPL_DECLARE(TcpClient)

public:
    static Coro<TcpClient> create();

    Coro<TcpConnection> connect(const char* host, short port);
};

class TcpConnection {
    friend class TcpClient;

    CLASS_PIMPL_DECLARE(TcpConnection)

public:
    Coro<size_t> send(const char* buf, size_t len);
    Coro<size_t> receive(char* buf, size_t len);
};

}