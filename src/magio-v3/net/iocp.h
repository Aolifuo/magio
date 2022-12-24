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

    void connect(IoContext& ioc) override;

    void accept(Socket& listener, IoContext& ioc) override;

    void send(IoContext& ioc) override;

    void receive(IoContext& ioc) override;

    void send_to(IoContext& ioc) override;

    void receive_from(IoContext& ioc) override;
    
    void relate(void* sock_handle, std::error_code& ec) override;

    int poll(size_t ms, std::error_code& ec) override;

    void wake_up() override;

private:
    struct Data;
    Data* data_;
};

}

}

#endif