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
    virtual Excepted<> post_receive_task(SocketHelper) = 0;
    virtual Excepted<> post_send_task(SocketHelper) = 0;
    virtual Excepted<> recycle(SocketHelper) = 0;
    virtual ~IocpInterface() = default;
private:
};

class IocpServer: public IocpInterface {

    CLASS_PIMPL_DECLARE(IocpServer)

public:
    static Excepted<IocpServer> bind(const char* host, short port, TransportProtocol protocol = TransportProtocol::TCP);
    
    Excepted<> post_accept_task(SocketHelper, CompletionHandler*);
    Excepted<> post_receive_task(SocketHelper) override;
    Excepted<> post_send_task(SocketHelper) override;
    int wait_completion_task();

    Excepted<> associate_with(SocketHelper, CompletionHandler* h = nullptr);
    Excepted<> recycle(SocketHelper) override;
};

class IocpClient: public IocpInterface {

    CLASS_PIMPL_DECLARE(IocpClient)

public:
    static Excepted<IocpClient> create();

    Excepted<> post_connect_task(const char* host1, short port1, 
                                const char* host2, short port2, CompletionHandler*);
    Excepted<> post_send_task(SocketHelper) override;
    Excepted<> post_receive_task(SocketHelper) override;
    int wait_completion_task();

    Excepted<> associate_with(SocketHelper, CompletionHandler* h = nullptr);
    Excepted<> recycle(SocketHelper) override;
};

} // namespace plat

} // namespace magio