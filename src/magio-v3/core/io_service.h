#ifndef MAGIO_CORE_IO_SERVICE_H_
#define MAGIO_CORE_IO_SERVICE_H_

#include <system_error>

namespace magio {

namespace net {

class Socket;

class IpAddress;

}

struct IoContext;

class IoService {
public:
    virtual ~IoService() = default;

    virtual void read_file(IoContext& ioc, size_t offset) = 0;

    virtual void write_file(IoContext& ioc, size_t offset) = 0;

    virtual void connect(IoContext& ioc) = 0;

    virtual void accept(net::Socket& listener, IoContext& ioc) = 0;

    virtual void send(IoContext& ioc) = 0;

    virtual void receive(IoContext& ioc) = 0;

    virtual void send_to(IoContext& ioc) = 0;

    virtual void receive_from(IoContext& ioc) = 0;

    virtual void cancel(IoContext& ioc) = 0;

    virtual void relate(void* handle, std::error_code& ec) = 0;

    // -1->big error, 0->wait timeout; 1->io; 2->continue
    virtual int poll(bool block, std::error_code& ec) = 0;

    virtual void wake_up() = 0;
};

}

#endif