#pragma once

#include "magio/plat/declare.h"
#include "magio/core/Pimpl.h"
#include "magio/coro/UseCoro.h"
#include "magio/net/Address.h"

namespace magio {

class TcpStream {
    friend class TcpServer;
    friend class TcpClient;

    CLASS_PIMPL_DECLARE(TcpStream)

public:
    Address local_address();
    Address remote_address();

    Coro<std::tuple<char*, size_t>> read();
    Coro<size_t> write(const char* data, size_t len);
};

class TcpServer {

    CLASS_PIMPL_DECLARE(TcpServer)

public:
    static Coro<TcpServer> bind(const char* host, uint_least16_t port);
    Coro<TcpStream> accept();
};

class TcpClient {

    CLASS_PIMPL_DECLARE(TcpClient)

public:

    Coro<TcpStream> connect(const char* host, uint_least16_t port);
};

}