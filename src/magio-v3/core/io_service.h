#ifndef MAGIO_CORE_IO_SERVICE_H_
#define MAGIO_CORE_IO_SERVICE_H_

#include <system_error>

#include "magio-v3/core/common.h"
#include "magio-v3/net/protocal.h"

namespace magio {

namespace net {

class Socket;

class InetAddress;

}

struct IoContext;

class IoServiceInterface {
public:
    using Cb = void(*)(std::error_code, IoContext*, void*);

    virtual ~IoServiceInterface() = default;

    virtual void write_file(IoHandle ioh, size_t offset, IoContext* ioc) = 0;

    virtual void read_file(IoHandle ioh, size_t offset, IoContext* ioc) = 0;

    virtual void accept(const net::Socket& listener, IoContext* ioc) = 0;

    virtual void connect(SocketHandle socket, IoContext* ioc) = 0;

    virtual void send(SocketHandle socket, IoContext* ioc) = 0;

    virtual void receive(SocketHandle socket, IoContext* ioc) = 0;

    virtual void send_to(SocketHandle socket, IoContext* ioc) = 0;

    virtual void receive_from(SocketHandle socket, IoContext* ioc) = 0;

    virtual void cancel(IoHandle ioh) = 0;

    virtual void attach(IoHandle ioh, std::error_code& ec) = 0;

    // -1->big error, 0->wait timeout; 1->io; 2->continue
    virtual int poll(size_t nanosec, std::error_code& ec) = 0;

    virtual void wake_up() = 0;
};

class IoService {
public:
    using Cb = void(*)(std::error_code, IoContext*, void*);

    IoService(IoServiceInterface* impl)
        : impl_(impl)
    { }

    void write_file(IoHandle ioh, const char* msg, size_t len, size_t offset, void* user_ptr, Cb);

    void read_file(IoHandle ioh, char* buf, size_t len, size_t offset, void* user_ptr, Cb);

    void accept(const net::Socket& listener, void* user_ptr, Cb);

    void connect(SocketHandle socket, const net::InetAddress& remote, void* user_ptr, Cb);

    void send(SocketHandle socket, const char* msg, size_t len, void* user_ptr, Cb);

    void receive(SocketHandle socket, char* buf, size_t len, void* user_ptr, Cb);

    void send_to(SocketHandle socket, const net::InetAddress& remote, const char* msg, size_t len, void* user_ptr, Cb);

    void receive_from(SocketHandle socket, char* buf, size_t len, void* user_ptr, Cb);

    void cancel(IoHandle ioh);
    
    void attach(IoHandle ioh, std::error_code& ec);

    int poll(size_t nanosec, std::error_code& ec);

    void wake_up();

private:
    IoServiceInterface* impl_;
};

}

#endif