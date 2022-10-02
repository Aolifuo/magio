#include "magio/plat/iocp/iocp.h"

#include <iostream>
#include <WS2tcpip.h>
#include <WinSock2.h>
#include <MSWSock.h>

#include "magio/plat/iocp/socket.h"
#include "magio/plat/errors.h"

#pragma comment(lib, "ws2_32.lib")

namespace magio {

namespace plat {

// IocpServer
struct IocpServer::Impl {
    Borrowed<SocketServer::pool_type> listen_socket;
    HANDLE iocp_handle;
    LPFN_ACCEPTEX async_accept;
    LPFN_GETACCEPTEXSOCKADDRS parse_accept;

    RingQueue<CompletionHandler*> listener_que{};

    ~Impl() {
        ::CloseHandle(iocp_handle);
    }
};

CLASS_PIMPL_IMPLEMENT(IocpServer)

Expected<IocpServer> IocpServer::bind(const char* host, short port,
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

    Impl* impl = new Impl{std::move(listener), HANDLE(iocp_handle),
                          (LPFN_ACCEPTEX)async_accept,
                          (LPFN_GETACCEPTEXSOCKADDRS)parse_accept};

    IocpServer server(impl);

    return {std::move(server)};
}

Expected<> IocpServer::associate_with(SocketHelper sock, CompletionHandler* handler) {
    HANDLE associate_result = ::CreateIoCompletionPort(
        (HANDLE)sock.handle(), impl->iocp_handle, (ULONG_PTR)handler, 0);

    if (!associate_result) {
        return {Error(last_error(),
                      "Failed to associate iocp with remote socket")};
    }

    return {Unit()};
}

Expected<> IocpServer::post_accept_task(SocketHelper sock, CompletionHandler* handler) {
    if (!sock.get()) {
        if (auto res = SocketServer::instance().make_socket(); !res) {
            return res.unwrap_err();
        } else {
            sock = res.unwrap().unwrap();
        }
    }
    sock.for_async_task(IOOperation::Accept);

    if (auto res = associate_with(sock, handler); !res) {
        unchecked_return_to_pool(sock.get(), SocketServer::instance().pool());
        return res.unwrap_err();
    }

    impl->listener_que.push(handler);

    // async_accept
    bool result = impl->async_accept(
        unpack(impl->listen_socket).handle(), 
        sock.handle(),
        sock.recv_io().buf(),
        0,
        sizeof(SOCKADDR_IN) + 16, // local
        sizeof(SOCKADDR_IN) + 16, // remote
        NULL,
        (LPOVERLAPPED)sock.recv_io().overlapped()
    );

    if (!result && ERROR_IO_PENDING != last_error()) {
        unchecked_return_to_pool(sock.get(), SocketServer::instance().pool());
        return Error(last_error(), "Post accept task failed");
    }

    return {Unit()};
}

Expected<> IocpServer::post_receive_task(SocketHelper sock_helper) {
    // associate_with(sock_helper, handler);

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
Expected<> IocpServer::post_send_task(SocketHelper sock_helper) {

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
            if (!impl->listener_que.empty()) {
                LPSOCKADDR local_addr_ptr;
                LPSOCKADDR remote_addr_ptr;
                int local_addr_len = 0;
                int remote_addr_len = 0;

                impl->parse_accept(
                    io.buf(),
                    0,
                    sizeof(SOCKADDR_IN) + 16,
                    sizeof(SOCKADDR_IN) + 16,
                    &local_addr_ptr,
                    &local_addr_len,
                    &remote_addr_ptr,
                    &remote_addr_len
                );

                char buff[40]{};
                auto local = (SOCKADDR_IN*)local_addr_ptr;
                auto remote = (SOCKADDR_IN*)remote_addr_ptr;

                ::inet_ntop(local->sin_family, &local->sin_addr, buff, 40);
                io.owner().local_addr() = {::ntohs(local->sin_port), buff};

                ZeroMemory(buff, sizeof(buff));

                ::inet_ntop(remote->sin_family, &remote->sin_addr, buff, 40);
                io.owner().remote_addr() = {::ntohs(remote->sin_port), buff};

                auto handler = impl->listener_que.front();
                impl->listener_que.pop();
                handler->cb(handler->hook, error, io.owner());
            } else {
                recycle(io.owner());
            }
            break;
        case IOOperation::Receive:
        case IOOperation::Send:
            if (io.len() == 0) {
                error.code = -1;
                error.msg = "Client disconnected";
            }
            handler->cb(handler->hook, error, io.owner());
            break;
        default:
            recycle(io.owner());
            break;
        }

        if (error) {
            // recycle(io.owner());
        }
    }

    return error.code;
}



Expected<> IocpServer::recycle(SocketHelper sock) {
    unchecked_return_to_pool(sock.get(), SocketServer::instance().pool());
    return Unit();
}


// IocpClient

struct IocpClient::Impl {
    HANDLE iocp_handle;
    LPFN_CONNECTEX async_connect;

    ~Impl() {
        ::CloseHandle(iocp_handle);
    }
};

CLASS_PIMPL_IMPLEMENT(IocpClient)

Expected<IocpClient> IocpClient::create() {
    auto may_sock = SocketServer::instance().make_socket();
    if (!may_sock) {
        return may_sock.unwrap_err();
    }
    auto sock = may_sock.unwrap();

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

    HANDLE iocp_handle =
        ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);

    if (!iocp_handle) {
        return {Error(last_error(), "Failed to create iocp")};
    }

    return {new Impl{iocp_handle, (LPFN_CONNECTEX)async_connect}};
}

Expected<> IocpClient::post_connect_task(const char* host1, short port1, 
                                        const char* host2, short port2, CompletionHandler* handler) 
{
    auto may_sock = SocketServer::instance().make_socket();
    if (!may_sock) {
        return may_sock.unwrap_err();
    }
    auto sock = may_sock.unwrap();

    if (auto res = unpack(sock).bind(host1, port1); !res) {
        return res.unwrap_err();
    }

    if (auto res = associate_with(sock.get(), handler); !res) {
        return res.unwrap_err();
    }

    unpack(sock).for_async_task(IOOperation::Connect);

    SOCKADDR_IN sockaddr;
    sockaddr.sin_family = AF_INET;
    sockaddr.sin_port = ::htons(port2);
    ::inet_pton(AF_INET, host2, &sockaddr.sin_addr.S_un.S_addr);

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

Expected<> IocpClient::post_send_task(SocketHelper sock) {
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

Expected<> IocpClient::post_receive_task(SocketHelper sock) {
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
            if (handler) {
                // char buff[40]{};
                // SOCKADDR_IN local_addr;
                // SOCKADDR_IN remote_addr;
                // int local_addr_len = 0;
                // int remote_addr_len = 0;

                // ::getsockname(io.owner().handle(), (SOCKADDR*)&local_addr, &local_addr_len);
                // ::getpeername(io.owner().handle(), (SOCKADDR*)&remote_addr, &remote_addr_len);

                // ::inet_ntop(local_addr.sin_family, &local_addr.sin_addr, buff, 40);
                // io.owner().local_addr() = {::ntohs(local_addr.sin_port), buff};

                // ZeroMemory(buff, sizeof(buff));

                // ::inet_ntop(remote_addr.sin_family, &remote_addr.sin_addr, buff, 40);
                // io.owner().remote_addr() = {::ntohs(remote_addr.sin_port), buff};

                handler->cb(handler->hook, error, io.owner());
            } else {
                recycle(io.owner());
            }
            break;
        case IOOperation::Receive:
        case IOOperation::Send:
            if (io.len() == 0) {
                error.code = -1;
                error.msg = "Server disconnected";
            }
            handler->cb(handler->hook, error, io.owner());
            break;
        default:
            recycle(io.owner());
            break;
        }

        if (error) {
            // recycle(io.owner());
        }
    }

    return error.code;
}


Expected<> IocpClient::associate_with(SocketHelper sock, CompletionHandler* handler) {
    HANDLE result = ::CreateIoCompletionPort(
        (HANDLE)sock.handle(), impl->iocp_handle, (ULONG_PTR)handler, 0);

    if (!result) {
        return {Error(last_error(), "Failed to associate iocp with reomote socket")};
    }

    return {Unit()};
}

Expected<> IocpClient::recycle(SocketHelper sock) {
    unchecked_return_to_pool(sock.get(), SocketServer::instance().pool());
    return Unit();
}

}  // namespace plat

}  // namespace magio
