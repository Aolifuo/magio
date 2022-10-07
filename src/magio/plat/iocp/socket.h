#pragma once

#ifdef _WIN32

#include "magio/core/Pool.h"
#include "magio/core/Error.h"
#include "magio/plat/declare.h"
#include "magio/net/tcp/Tcp.h"

namespace magio {

namespace plat {

struct IOContext;

struct Socket;

struct sockaddr_in;

class SocketHelper;

class IOContextHelper {
public:
    IOContextHelper(IOContext*);

    void* overlapped();
    SocketHelper owner();
    void* wsa_buf();
    char* buf();
    unsigned capacity();
    unsigned len();
    IOOperation io_operation();

    void reset_overlapped();
    void set_io_operation(IOOperation);
    void set_len(unsigned);

    auto get() { return ioc_; }

private:
    IOContext* ioc_ = nullptr;
};

class SocketHelper {
public:

    SocketHelper(Socket*);
    SocketHelper& operator=(Socket* sock) {
        sock_ = sock;
        return *this;
    }

    socket_type handle();
    void* address();
    Address& local_addr();
    Address& remote_addr();
    IOContextHelper recv_io();
    IOContextHelper send_io();
    void set_handle(socket_type);
    void set_address(void*);
    void set_recv_len(unsigned len);
    void set_send_len(unsigned len);
    void for_async_task(IOOperation);

    Socket* get() { return sock_; }

    Expected<> bind(const char* host, short port);
    Expected<> listen();
private:
    Socket* sock_ = nullptr;
};

class SocketServer {
    SocketServer();

public:
    using pool_type = BlockedFixedPool<Socket>;

    SocketServer(const SocketServer&) = delete;
    SocketServer(SocketServer&&) = delete;
    ~SocketServer();

    static Expected<> initialize();
    static SocketServer& instance();
    static void close();

    Expected<Borrowed<pool_type>> make_socket(TransportProtocol protocol = TransportProtocol::TCP);
    Expected<Borrowed<pool_type>> raw_socket();
    Expected<> replace_socket(
        Socket* old, TransportProtocol protocol = TransportProtocol::TCP);
    pool_type* pool();

private:

    struct Data;
    Data* data;
};

int last_error();

template <typename Pool>
inline SocketHelper unpack(Borrowed<Pool>& borrowed) {
    return {borrowed.get()};
}

inline SocketHelper unpack(Socket* sock) { return {sock}; }

inline IOContextHelper unpack(IOContext* ioc) { return {ioc}; }

}  // namespace plat

}  // namespace magio
#endif