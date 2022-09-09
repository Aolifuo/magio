#include "magio/plat/iocp.h"

#include <WS2tcpip.h>
#include <WinSock2.h>
#include <MSWSock.h>
#include <functional>

#include "magio/core/Log.h"
#include "magio/plat/socket.h"

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

// 顺序保证
Excepted<> IocpServer::post_receive_task(SocketHelper sock_helper, CompletionHandler* handler) {
    // 
    associate_with(sock_helper, handler);

    IOContextHelper io_helper = sock_helper.recv_io();
    sock_helper.for_async_task(IOOperation::Receive);
    DWORD flag = 0;

    int status =
        ::WSARecv(sock_helper.handle(), (LPWSABUF)io_helper.wsa_buf(), 1, NULL,
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
        ::WSASend(sock_helper.handle(), (LPWSABUF)io_helper.wsa_buf(), 1, NULL,
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
        int err_no = last_error();

        switch (err_no) {
            case WAIT_TIMEOUT:
                // 等待超时
                error = {Error{err_no, "The wait operation timed out"}};
                break;
            case ERROR_NETNAME_DELETED:
                // 对面连接异常关闭
                error = {Error{
                    err_no, "The specified network name is no longer available."}};
                break;
            case ERROR_CONNECTION_ABORTED:
                // 服务器主动断开连接
                error = {Error{err_no, "The operation could not be completed. A retry should be performed."}};
                break;
            case ERROR_OPERATION_ABORTED:
                // 使用 shutdown 收尾
                error = {
                    Error{err_no,
                            "The I/O operation has been aborted because of "
                            "either a thread exit or an application request"}};
                break;
            case ERROR_ABANDONED_WAIT_0:
                // 在等待I/O完成过程中iocp被关闭 (如 CloseHandle)
                error = {Error{err_no, "Iocp service is interrupted"}};
                break;
            case ERROR_INVALID_HANDLE:
                // 试图访问已被关闭的或不存在的iocp服务
                error = {Error{err_no, "Iocp service does not exist"}};
                break;
            default:
                // 其他未知情况
                error = {Error{err_no, "Unknown iocp message"}};
                break;
        }
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

}  // namespace plat

}  // namespace magio
