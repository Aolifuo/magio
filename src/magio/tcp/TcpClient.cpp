#include "magio/tcp/TcpClient.h"

#include <memory>
#include "magio/coro/Coro.h"
#include "magio/coro/ThisCoro.h"
#include "magio/plat/iocp.h"
#include "magio/plat/socket.h"
#include <winerror.h>

namespace magio {

struct ConnectHook {
    Error err;
    plat::SocketHelper sock{nullptr};
    std::coroutine_handle<> h;
    AnyExecutor exe;
};


struct TcpClient::Impl {
    plat::IocpClient iocp_client;
};

struct TcpConnection::Impl {
    plat::IocpClient* iocp_client;
    plat::SocketHelper socket;

    ConnectHook hook;
    std::unique_ptr<plat::CompletionHandler> handler;

    Impl(plat::IocpClient* client, plat::SocketHelper sock, std::unique_ptr<plat::CompletionHandler> h)
        : iocp_client(client), socket(sock), handler(std::move(h)) 
    {
        handler->hook = &hook;
        handler->cb = [](void* hook, Error err, plat::SocketHelper sock) {
            auto chook = (ConnectHook*)hook;
            if (err) {
                chook->err = err;
            }
            chook->exe.post([h = chook->h] { h.resume(); });
        };
    }

    ~Impl() {
        unchecked_return_to_pool(socket.get(), plat::SocketServer::instance().pool());
    }
};

CLASS_PIMPL_IMPLEMENT(TcpClient)

Coro<TcpClient> TcpClient::create() {
    if (auto res = plat::IocpClient::create(); res) {
        auto impl = new Impl{res.unwrap()};

        auto exe = co_await this_coro::executor;
        exe.waiting([impl] {
            int status = impl->iocp_client.wait_completion_task();
            if (status == ERROR_ABANDONED_WAIT_0 
                || status == ERROR_INVALID_HANDLE
            ) {
                return true;
            }

            return false;
        });

        co_return {impl};
    } else {
        throw std::runtime_error(res.unwrap_err().msg);
    }
}

Coro<TcpConnection> TcpClient::connect(const char *host, short port) {
    auto exe = co_await this_coro::executor;
    ConnectHook hook{.exe = exe};

   auto handler_ptr = std::make_unique<plat::CompletionHandler>(plat::CompletionHandler{
        .hook = (void*)&hook,
        .cb = [](void* hook, Error err, plat::SocketHelper sock) {
            auto chook = (ConnectHook*)hook;
            if (err) {
                chook->err = err;
            }
            chook->sock = sock;
            chook->exe.post([=] { chook->h.resume(); });
        }
    });

    if (auto res = impl->iocp_client.post_connect_task(host, port, handler_ptr.get()); !res) {
        throw std::runtime_error(res.unwrap_err().msg);
    }

    co_await Coro<void> {
        [&hook, handler = handler_ptr.get(), impl = impl](std::coroutine_handle<> h) {
            hook.h = h;
            impl->iocp_client.add_connector(handler);
        }
    };

    if (hook.err) {
        throw std::runtime_error(hook.err.msg);
    }

    auto conn_impl
         = new TcpConnection::Impl{&impl->iocp_client, hook.sock, std::move(handler_ptr)};
    co_return {conn_impl};
}

CLASS_PIMPL_IMPLEMENT(TcpConnection)

Coro<size_t> TcpConnection::send(const char *buf, size_t len) {
    auto exe = co_await this_coro::executor;
    impl->hook.exe = exe;

    size_t cp_len = impl->socket.send_io().capacity() > len ? len : impl->socket.send_io().capacity();
    std::memcpy(impl->socket.send_io().buf(), buf, cp_len);
    impl->socket.send_io().set_len(cp_len);

    co_await Coro<void>{
        [impl = impl](std::coroutine_handle<> h) {
            impl->hook.h = h;
            impl->iocp_client->post_send_task(impl->socket);
        }
    };

    if (impl->hook.err) {
        throw std::runtime_error(impl->hook.err.msg);
    }

    co_return impl->socket.send_io().len();
}

Coro<size_t> TcpConnection::receive(char *buf, size_t len) {
    auto exe = co_await this_coro::executor;
    impl->hook.exe = exe;

    co_await Coro<void>{
        [impl = impl](std::coroutine_handle<> h) {
            impl->hook.h = h;
            impl->iocp_client->post_receive_task(impl->socket);
        }
    };
    
    if (impl->hook.err) {
        throw std::runtime_error(impl->hook.err.msg);
    }

    size_t ret_len = impl->socket.recv_io().len() > len ? len : impl->socket.recv_io().len(); 
    std::memcpy(buf, impl->socket.recv_io().buf(), ret_len);

    co_return ret_len;
}

}