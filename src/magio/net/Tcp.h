#pragma once

#include "magio/coro/Fwd.h"
#include "magio/net/Socket.h"

namespace magio {

class TcpStream {
    friend class TcpServer;

public:
    TcpStream(Socket&& socket, const SocketAddress& local, const SocketAddress& remote)
        : socket_(std::move(socket))
        , local_(local)
        , remote_(remote)
    { }

    SocketAddress local_address();
    SocketAddress remote_address();

    static Coro<TcpStream> connect(const char* host, uint_least16_t port);

    Coro<size_t> read(char* buf, size_t len);
    Coro<size_t> write(const char* buf, size_t len);

    AnyExecutor get_executor();
private:
    Socket socket_;
    SocketAddress local_;
    SocketAddress remote_;
};

class TcpServer {
public:
    TcpServer(Socket&& socket)
        : listener(std::move(socket))
    { }

    static Coro<TcpServer> bind(const char* host, uint_least16_t port);
    Coro<TcpStream> accept();

    AnyExecutor get_executor();

private:
    Socket listener;
};


}