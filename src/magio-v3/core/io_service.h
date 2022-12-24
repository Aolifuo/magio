#ifndef MAGIO_CORE_IO_SERVICE_H_
#define MAGIO_CORE_IO_SERVICE_H_

#include <system_error>

namespace magio {

namespace net {

class Socket;

class IpAddress;

struct IoContext;

}

class IoService {
public:
    virtual ~IoService() = default;

    virtual void connect(net::IoContext& ioc) = 0;

    virtual void accept(net::Socket& listener, net::IoContext& ioc) = 0;

    virtual void send(net::IoContext& ioc) = 0;

    virtual void receive(net::IoContext& ioc) = 0;

    virtual void send_to(net::IoContext& ioc) = 0;

    virtual void receive_from(net::IoContext& ioc) = 0;

    virtual void relate(void* handle, std::error_code& ec) = 0;

    // -1->big error, 0->wait timeout; 1->io; 2->wake up
    virtual int poll(size_t ms, std::error_code& ec) = 0;

    virtual void wake_up() = 0;
};

}

#endif