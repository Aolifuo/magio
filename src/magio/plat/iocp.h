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


class IocpServer {
    CLASS_PIMPL_DECLARE(IocpServer)
public:
    
    static Excepted<IocpServer> bind(const char* host, short port, TransportProtocol protocol = TransportProtocol::TCP);
    
    Excepted<> post_accept_task(SocketHelper);
    Excepted<> post_receive_task(SocketHelper, CompletionHandler*);
    Excepted<> post_send_task(SocketHelper);
    int wait_completion_task();
    void add_listener(CompletionHandler*);

    Excepted<> repost_accept_task(SocketHelper);
private:
    Excepted<> associate_with(SocketHelper, CompletionHandler* h = nullptr);
    
};

} // namespace plat

} // namespace magio