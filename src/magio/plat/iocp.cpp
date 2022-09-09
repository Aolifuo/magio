#include "magio/plat/iocp.h"

#include <WS2tcpip.h>
#include <WinSock2.h>
#include <MSWSock.h>

#include "magio/core/Log.h"
#include "magio/plat/socket.h"
#include "magio/plat/system_errors.h"

#pragma comment(lib, "ws2_32.lib")

template <typename CharT>
struct std::formatter<magio::plat::Message, CharT>
    : std::formatter<int, CharT> {
    template <typename FormatContext>
    auto format(const magio::plat::Message& msg, FormatContext& fc) {
        return std::format_to(fc.out(), "code:{}. {}", msg.code, msg.msg);
    }
};

template <typename CharT>
struct std::formatter<magio::plat::IOOperation, CharT>
    : std::formatter<int, CharT> {
    template <typename FormatContext>
    auto format(magio::plat::IOOperation ioop, FormatContext& fc) {
        string_view op_str;
        switch (ioop) {
            case magio::plat::IOOperation::Accept:
                op_str = "accept";
                break;
            case magio::plat::IOOperation::Receive:
                op_str = "receive";
                break;
            case magio::plat::IOOperation::Send:
                op_str = "send";
                break;
            case magio::plat::IOOperation::Noop:
                op_str = "noop";
                break;
            case magio::plat::IOOperation::Connect:
                op_str = "connect";
        }
        return std::format_to(fc.out(), "{}", op_str);
    }
};

namespace magio {

namespace plat {

// IocpServer
struct IocpServer::Impl {
    Borrowed<SocketServer::pool_type> listen_socket;
    HANDLE iocp_handle;
    LPFN_ACCEPTEX async_accept;
    LPFN_GETACCEPTEXSOCKADDRS parse_accept;

    RingQueue<CompletionHandler*> listener_queue;

    ~Impl() {
        ::CloseHandle(iocp_handle);
    }
};

CLASS_PIMPL_IMPLEMENT(IocpServer)

Excepted<IocpServer> IocpServer::bind(const char* host, short port,
                                       TransportProtocol protocol) {
    auto maybe_listener = SocketServer::instance().make_socket(protocol);
    if (!maybe_listener) {
        return maybe_listener.unwrap_err();
    }
    auto listener = maybe_listener.unwrap();
    auto helper = unpack(listener);

    if (auto bind_result = helper.bind(host, port); !bind_result) {
        return bind_result.unwrap_err();
    }

    if (auto listen_res = helper.listen(); !listen_res) {
        return listen_res.unwrap_err();
    }

    HANDLE iocp_handle =
        ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);

    if (!iocp_handle) {
        return {Error(last_error(), "Failed to create iocp")};
    }

    HANDLE associate_result =
        ::CreateIoCompletionPort((HANDLE)helper.handle(), 
                                iocp_handle, 
                                (ULONG_PTR)114514, 0);
    if (!associate_result) {
        return Error(last_error(), "Failed to associate iocp with listen socket");
    }

    // load AcceptEx
    GUID GuidAcceptEx = WSAID_ACCEPTEX;
    void* async_accept = NULL;
    DWORD ret_bytes;
    int load_result1 =
        ::WSAIoctl(helper.handle(), SIO_GET_EXTENSION_FUNCTION_POINTER,
                   &GuidAcceptEx, sizeof(GuidAcceptEx), &async_accept,
                   sizeof(async_accept), &ret_bytes, NULL, NULL);

    if (0 != load_result1) {
        return {Error(last_error(), "Failed to get 'AcceptEx' pointer")};
    }

    GUID GuidGetAcceptExSockAddrs = WSAID_GETACCEPTEXSOCKADDRS;
    void* parse_accept;
    int load_result2 =
        ::WSAIoctl(helper.handle(), SIO_GET_EXTENSION_FUNCTION_POINTER,
                   &GuidGetAcceptExSockAddrs, sizeof(GuidGetAcceptExSockAddrs),
                   &parse_accept, sizeof(parse_accept), &ret_bytes, NULL, NULL);

    if (0 != load_result2) {
        return {Error(last_error(),
                      "Failed to get 'GetAcceptExSockAddrs' pointer")};
    }

    Impl* impl = new Impl{std::move(listener), HANDLE(iocp_handle),
                          (LPFN_ACCEPTEX)async_accept,
                          (LPFN_GETACCEPTEXSOCKADDRS)parse_accept,
                          RingQueue<CompletionHandler*>()};

    IocpServer server(impl);

    return {std::move(server)};
}

Excepted<> IocpServer::associate_with(SocketHelper sock, CompletionHandler* handler) {
    HANDLE associate_result = ::CreateIoCompletionPort(
        (HANDLE)sock.handle(), impl->iocp_handle, (ULONG_PTR)handler, 0);

    if (!associate_result) {
        return {Error(last_error(),
                      "Failed to associate iocp with reomote socket")};
    }

    return {Unit()};
}

Excepted<> IocpServer::post_accept_task(SocketHelper sock) {
    if (!sock.get()) {
        if (auto res = SocketServer::instance().make_socket(); !res) {
            return res.unwrap_err();
        } else {
            sock = res.unwrap().unwrap();
        }
    }

    sock.for_async_task(IOOperation::Accept);

    // async_accept
    bool result =
        impl->async_accept(unpack(impl->listen_socket).handle(), sock.handle(),
                           sock.recv_io().buf(), 0, sizeof(SOCKADDR_IN) + 16,
                           sizeof(SOCKADDR_IN) + 16, NULL,
                           (LPOVERLAPPED)sock.recv_io().overlapped());

    if (!result && ERROR_IO_PENDING != last_error()) {
        unchecked_return_to_pool(sock.get(), SocketServer::instance().pool());
        return Error(last_error(), "Post accept task failed");
    }

    return {Unit()};
}

Excepted<> IocpServer::post_receive_task(SocketHelper sock_helper, CompletionHandler* handler) {
    // 
    associate_with(sock_helper, handler);

    IOContextHelper io_helper = sock_helper.recv_io();
    sock_helper.for_async_task(IOOperation::Receive);
    DWORD flag = 0;

    int status =
        ::WSARecv(sock_helper.handle(), (LPWSABUF)io_helper.wsa_buf(),
                    1, NULL,
                    &flag, (LPOVERLAPPED)io_helper.overlapped(), NULL);

    if (SOCKET_ERROR == status && ERROR_IO_PENDING != last_error()) {
        return Error(last_error(), "Failed to post receive task");
    }

    return {Unit()};
}

// 
Excepted<> IocpServer::post_send_task(SocketHelper sock_helper) {

    IOContextHelper io_helper = sock_helper.send_io();
    sock_helper.for_async_task(IOOperation::Send);
    DWORD flag = 0;

    int status =
        ::WSASend(sock_helper.handle(), (LPWSABUF)io_helper.wsa_buf(),
                    1, NULL,
                    flag, (LPOVERLAPPED)io_helper.overlapped(), NULL);

    if (SOCKET_ERROR == status && ERROR_IO_PENDING != last_error()) {
        return Error(last_error(), "Failed to post send task");
    }

    return Unit();
}

int IocpServer::wait_completion_task() {
    DWORD bytes_nums;
    CompletionHandler* handler;
    IOContext* io_context = nullptr;

    bool status = ::GetQueuedCompletionStatus(
        impl->iocp_handle,
        &bytes_nums,
        (PULONG_PTR)&handler,
        (LPOVERLAPPED*)&io_context,
        0); // INFINITE

    Error error;
    if (!status) {
        error.code = last_error();
        error.msg = system_error_str(last_error());
    }

    if (io_context) {
        auto io = unpack(io_context);
        io.set_len(bytes_nums);

        switch(io.io_operation()) {
        case IOOperation::Accept:
            if (!impl->listener_queue.empty()) {
                auto listener = impl->listener_queue.front();
                listener->cb(listener->hook, error, io.owner());
                impl->listener_queue.pop();
                break;
            }
            repost_accept_task(io.owner());
            break;
        case IOOperation::Receive:
            handler->cb(handler->hook, error, io.owner());
            break;
        case IOOperation::Send:
            handler->cb(handler->hook, error, io.owner());
            break;
        default:
            break;
        }

        if (error) {
            repost_accept_task(io.owner());
        }
    }

    return error.code;
}

void IocpServer::add_listener(CompletionHandler* h) {
    impl->listener_queue.push(h);
}

Excepted<> IocpServer::repost_accept_task(SocketHelper sock) {
    SocketServer::instance().replace_socket(sock.get());
    return post_accept_task(sock);
}


// IocpClient

struct IocpClient::Impl {
    HANDLE iocp_handle;
    LPFN_CONNECTEX async_connect;

    RingQueue<CompletionHandler*> connector_queue{};

    ~Impl() {
        ::CloseHandle(iocp_handle);
    }
};

CLASS_PIMPL_IMPLEMENT(IocpClient)

Excepted<IocpClient> IocpClient::create() {
    auto may_sock = SocketServer::instance().make_socket();
    if (!may_sock) {
        return may_sock.unwrap_err();
    }
    auto sock = may_sock.unwrap();

    HANDLE iocp_handle =
        ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);

    if (!iocp_handle) {
        return {Error(last_error(), "Failed to create iocp")};
    }
    
    GUID GUidConnectEx = WSAID_CONNECTEX;
    void* async_connect = NULL;
    DWORD ret_bytes;
    int load_result =
        ::WSAIoctl(unpack(sock).handle(), SIO_GET_EXTENSION_FUNCTION_POINTER,
                   &GUidConnectEx, sizeof(GUidConnectEx), 
                   &async_connect, sizeof(async_connect), 
                   &ret_bytes, NULL, NULL);

    if (0 != load_result) {
        return {Error(last_error(), "Failed to get 'ConnectEx' pointer")};
    }

    return {new Impl{iocp_handle, (LPFN_CONNECTEX)async_connect}};
}

Excepted<> IocpClient::post_connect_task(const char* host, short port, CompletionHandler* handler) {
    auto may_b = SocketServer::instance().make_socket();
    if (!may_b) {
        return may_b.unwrap_err();
    }
    auto sock = may_b.unwrap();

    if (auto res = unpack(sock).bind(host, 8081); !res) {
        return res.unwrap_err();
    }

    if (auto res = associate_with(sock.get(), handler); !res) {
        return res.unwrap_err();
    }
    
    unpack(sock).for_async_task(IOOperation::Connect);

    SOCKADDR_IN sockaddr;
    sockaddr.sin_family = AF_INET;
    sockaddr.sin_port = ::htons(port);
    ::inet_pton(AF_INET, host, &sockaddr.sin_addr.S_un.S_addr);

    DWORD bytes_send;
    auto result = impl->async_connect(
        unpack(sock).handle(),
        (SOCKADDR*)&sockaddr,
        sizeof(sockaddr),
        NULL,
        0,
        (LPDWORD)&bytes_send,
        (LPOVERLAPPED)unpack(sock).send_io().overlapped()
    );

    if (!result && ERROR_IO_PENDING != last_error()) {
        return {Error("Failed to connect to target")};
    }

    sock.unwrap();
    return Unit();
}

Excepted<> IocpClient::post_send_task(SocketHelper sock) {
    IOContextHelper io_helper = sock.send_io();
    sock.for_async_task(IOOperation::Send);
    DWORD flag = 0;

    int status =
        ::WSASend(sock.handle(), (LPWSABUF)io_helper.wsa_buf(),
                    1, NULL,
                    flag, (LPOVERLAPPED)io_helper.overlapped(), NULL);

    if (SOCKET_ERROR == status && ERROR_IO_PENDING != last_error()) {
        return Error(last_error(), "Failed to post send task");
    }

    
    return Unit();
}

Excepted<> IocpClient::post_receive_task(SocketHelper sock) {
    IOContextHelper io_helper = sock.recv_io();
    sock.for_async_task(IOOperation::Receive);
    DWORD flag = 0;

    int status =
        ::WSARecv(sock.handle(), (LPWSABUF)io_helper.wsa_buf(),
                    1, NULL,
                    &flag, (LPOVERLAPPED)io_helper.overlapped(), NULL);

    if (SOCKET_ERROR == status && ERROR_IO_PENDING != last_error()) {
        return Error(last_error(), "Failed to post receive task");
    }

    return {Unit()};
}

int IocpClient::wait_completion_task() {
    DWORD bytes_nums;
    CompletionHandler* handler;
    IOContext* io_context = nullptr;

    bool status = ::GetQueuedCompletionStatus(
        impl->iocp_handle,
        &bytes_nums,
        (PULONG_PTR)&handler,
        (LPOVERLAPPED*)&io_context,
        0); // INFINITE

    Error error;
    if (!status) {
        error.code = last_error();
        error.msg = system_error_str(last_error());
    }

    if (io_context) {
        auto io = unpack(io_context);
        io.set_len(bytes_nums);

        switch(io.io_operation()) {
        case IOOperation::Connect:
            if (!impl->connector_queue.empty()) {
                auto handler = impl->connector_queue.front();
                handler->cb(handler->hook, error, io.owner());
                impl->connector_queue.pop();
                break;
            }
            unchecked_return_to_pool(io.owner().get(), SocketServer::instance().pool());
            break;
        case IOOperation::Receive:
            handler->cb(handler->hook, error, io.owner());
            break;
        case IOOperation::Send:
            handler->cb(handler->hook, error, io.owner());
            break;
        default:
            break;
        }

        if (error) {
            unchecked_return_to_pool(io.owner().get(), SocketServer::instance().pool());
        }
    }

    return error.code;
}

void IocpClient::add_connector(CompletionHandler * handler) {
    impl->connector_queue.push(handler);
}

Excepted<> IocpClient::associate_with(SocketHelper sock, CompletionHandler* handler) {
    HANDLE result = ::CreateIoCompletionPort(
        (HANDLE)sock.handle(), impl->iocp_handle, (ULONG_PTR)handler, 0);

    if (!result) {
        return {Error(last_error(), "Failed to associate iocp with reomote socket")};
    }

    return {Unit()};
}

}  // namespace plat

}  // namespace magio
