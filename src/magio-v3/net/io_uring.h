#ifndef MAGIO_NET_IO_URING_H_
#define MAGIO_NET_IO_URING_H_

#include "magio-v3/core/noncopyable.h"
#include "magio-v3/core/io_service.h"

struct io_uring;

namespace magio {

namespace net {

class IoUring: Noncopyable, public IoService {
public:
    IoUring(unsigned entries);

    ~IoUring();

    void read_file(IoContext& ioc) override;

    void write_file(IoContext& ioc) override;

    void connect(IoContext& ioc) override;

    void accept(Socket& listener, IoContext& ioc) override;

    void send(IoContext& ioc) override;

    void receive(IoContext& ioc) override;

    void send_to(IoContext& ioc) override;

    void receive_from(IoContext& ioc) override;
    
    void relate(void* sock_handle, std::error_code& ec) override;

    int poll(bool block, std::error_code& ec) override;

    void wake_up() override;

private:
    io_uring* p_io_uring_ = nullptr;
};

}

}

#endif