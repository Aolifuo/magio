#ifndef MAGIO_NET_IOCP_H_
#define MAGIO_NET_IOCP_H_

#include "magio-v3/core/noncopyable.h"
#include "magio-v3/core/io_service.h"

namespace magio {

namespace net {

class IoCompletionPort: Noncopyable, public IoServiceInterface {
public:
    IoCompletionPort();

    ~IoCompletionPort();

    void write_file(IoHandle ioh, size_t offset, IoContext* ioc) override;
    
    void read_file(IoHandle ioh, size_t offset, IoContext* ioc) override;
    
    void accept(const net::Socket& listener, IoContext* ioc) override;
    
    void connect(SocketHandle socket, IoContext* ioc) override;

    void send(SocketHandle socket, IoContext* ioc) override;

    void receive(SocketHandle socket, IoContext* ioc) override;

    void send_to(SocketHandle socket, IoContext* ioc) override;

    void receive_from(SocketHandle socket, IoContext* ioc) override;

    void cancel(IoHandle ioh) override;
    
    void attach(IoHandle ioh, std::error_code& ec) override;

    int poll(size_t nanosec, std::error_code& ec) override;

    void wake_up() override;

private:
    struct Data;
    Data* data_;
};

}

}

#endif