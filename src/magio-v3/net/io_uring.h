#ifndef MAGIO_NET_IO_URING_H_
#define MAGIO_NET_IO_URING_H_

#include "magio-v3/core/noncopyable.h"
#include "magio-v3/core/io_service.h"

struct io_uring;

struct io_uring_cqe;

namespace magio {

namespace net {

constexpr size_t kCQEs = 1024;

class IoUring: Noncopyable, public IoService {
public:
    IoUring(unsigned entries);

    ~IoUring();

    void read_file(IoContext& ioc, size_t offset) override;

    void write_file(IoContext& ioc, size_t offset) override;

    void connect(IoContext& ioc) override;

    void accept(Socket& listener, IoContext& ioc) override;

    void send(IoContext& ioc) override;

    void receive(IoContext& ioc) override;

    void send_to(IoContext& ioc) override;

    void receive_from(IoContext& ioc) override;

    void cancel(IoContext& ioc) override;
    
    void relate(void* sock_handle, std::error_code& ec) override;

    int poll(size_t nanosec, std::error_code& ec) override;

    void wake_up() override;

private:
    void handle_cqe(io_uring_cqe*);

    void prep_wake_up();

    IoContext* wake_up_ctx_;
    io_uring_cqe* cqes_[kCQEs];
    size_t io_num_ = 0;
    io_uring* p_io_uring_ = nullptr;
};

}

}

#endif