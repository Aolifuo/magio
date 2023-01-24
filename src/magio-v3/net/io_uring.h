#ifndef MAGIO_NET_IO_URING_H_
#define MAGIO_NET_IO_URING_H_

#include "magio-v3/core/noncopyable.h"
#include "magio-v3/core/io_service.h"

struct io_uring;

struct io_uring_cqe;

namespace magio {

struct WakeupContext;

namespace net {

class IoUring: Noncopyable, public IoServiceInterface {
public:
    IoUring(unsigned entries);

    ~IoUring();
    
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
    void handle_cqe(io_uring_cqe*);

    void prep_wake_up();

    int wake_up_fd_;
    IoContext* wake_up_ctx_;
    IoContext* empty_ctx_;
    size_t io_num_ = 0;
    io_uring* p_io_uring_ = nullptr;
};

}

}

#endif