#pragma once

#include "magio/core/Pimpl.h"
#include "magio/core/Error.h"
#include "magio/plat/declare.h"

namespace magio {

namespace plat {
//
struct Message {
    int code;
    std::string_view msg;
};
struct CompletionHandler { 
    void* hook;
    void(*cb)(void *, Error, SocketHelper);
};

class IocpInterface {
public:
    virtual Expected<> post_receive_task(SocketHelper) = 0;
    virtual Expected<> post_send_task(SocketHelper) = 0;
    virtual Expected<> recycle(SocketHelper) = 0;
    virtual ~IocpInterface() = default;
private:
};

class IocpServer: public IocpInterface {

    CLASS_PIMPL_DECLARE(IocpServer)

public:
    static Expected<IocpServer> bind(const char* host, short port, TransportProtocol protocol = TransportProtocol::TCP);
    
    Expected<> post_accept_task(SocketHelper, CompletionHandler*);
    Expected<> post_receive_task(SocketHelper) override;
    Expected<> post_send_task(SocketHelper) override;
    int wait_completion_task();

    Expected<> associate_with(SocketHelper, CompletionHandler* h = nullptr);
    Expected<> recycle(SocketHelper) override;
};

class IocpClient: public IocpInterface {

    CLASS_PIMPL_DECLARE(IocpClient)

public:
    static Expected<IocpClient> create();

    Expected<> post_connect_task(const char* host1, short port1, 
                                const char* host2, short port2, CompletionHandler*);
    Expected<> post_send_task(SocketHelper) override;
    Expected<> post_receive_task(SocketHelper) override;
    int wait_completion_task();

    Expected<> associate_with(SocketHelper, CompletionHandler* h = nullptr);
    Expected<> recycle(SocketHelper) override;
};

} // namespace plat

} // namespace magio