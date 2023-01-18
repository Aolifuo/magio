#ifndef MAGIO_NET_IOCP_H_
#define MAGIO_NET_IOCP_H_

#include "magio-v3/core/noncopyable.h"
#include "magio-v3/core/io_service.h"

namespace magio {

namespace net {

class IoCompletionPort: Noncopyable, public IoService {
public:
    IoCompletionPort();

    ~IoCompletionPort();

    void read_file(IoContext& ioc, size_t offset) override;

    void write_file(IoContext& ioc, size_t offset) override;

    void connect(IoContext& ioc) override;

    void accept(Socket& listener, IoContext& ioc) override;

    void send(IoContext& ioc) override;

    void receive(IoContext& ioc) override;

    void send_to(IoContext& ioc) override;

    void receive_from(IoContext& ioc) override;

    void cancel(IoContext& ioc) override;
    
    void relate(void* handle, std::error_code& ec) override;

    int poll(size_t nanosec, std::error_code& ec) override;

    void wake_up() override;

private:
    struct Data;
    Data* data_;
};

}

}

#endif