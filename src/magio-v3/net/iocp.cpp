#ifdef _WIN32
#include "magio-v3/net/iocp.h"

#include "magio-v3/utils/logger.h"
#include "magio-v3/core/io_context.h"
#include "magio-v3/net/socket.h"

#include <MSWSock.h>

namespace magio {

namespace net {

class WebSocket {
public:
    static void init() {
        static WebSocket ws;
        WSADATA wsa_data;
        if (0 != ::WSAStartup(MAKEWORD(2, 2), &wsa_data)) {
            M_FATAL("{}", "failed to start wsa");
        }
    }

    ~WebSocket() {
        ::WSACleanup();
    }
};

struct IoCompletionPort::Data {
    HANDLE handle;

    LPFN_ACCEPTEX accept;
    LPFN_CONNECTEX connect;
    LPFN_GETACCEPTEXSOCKADDRS get_sock_addr;

    size_t io_num = 0;
};

IoCompletionPort::IoCompletionPort() {
    WebSocket::init();

    std::error_code ec;
    auto socket = Socket::open(Ip::v4, Transport::Tcp) | get_err(ec);
    if (ec) {
        M_FATAL("Failed to open a socket in iocp: {}", ec.message());
    }

    void* accept_ptr = nullptr;
    void* connect_ptr = nullptr;
    void* get_sock_addr_ptr = nullptr;

    GUID GuidAcceptEx = WSAID_ACCEPTEX;
    DWORD bytes_return;
    if (0 != ::WSAIoctl(
        socket.handle(), 
        SIO_GET_EXTENSION_FUNCTION_POINTER, 
        &GuidAcceptEx, 
        sizeof(GuidAcceptEx), 
        &accept_ptr, 
        sizeof(accept_ptr), 
        &bytes_return, 
        NULL, 
        NULL)
    ) {
        M_FATAL("Failed to get accept func: {}", SYSTEM_ERROR_CODE.value());
    }
    
    GUID GUidConnectEx = WSAID_CONNECTEX;
    if (0 != ::WSAIoctl(
        socket.handle(), 
        SIO_GET_EXTENSION_FUNCTION_POINTER, 
        &GUidConnectEx, 
        sizeof(GUidConnectEx), 
        &connect_ptr, 
        sizeof(connect_ptr), 
        &bytes_return, 
        NULL, 
        NULL)
    ) {
        M_FATAL("Failed to get connect func: {}", SYSTEM_ERROR_CODE.value());
    }

    GUID GuidGetAcceptExSockAddrs = WSAID_GETACCEPTEXSOCKADDRS;
    if (0 != ::WSAIoctl(
        socket.handle(), 
        SIO_GET_EXTENSION_FUNCTION_POINTER, 
        &GuidGetAcceptExSockAddrs, 
        sizeof(GuidGetAcceptExSockAddrs), 
        &get_sock_addr_ptr, 
        sizeof(get_sock_addr_ptr), 
        &bytes_return, 
        NULL, 
        NULL)
    ) {
        M_FATAL("Failed to get sock addr func: {}", SYSTEM_ERROR_CODE.value());
    }

    HANDLE handle = ::CreateIoCompletionPort(
        INVALID_HANDLE_VALUE, 
        NULL, 
        0, 
        1
    );

    if (!handle) {
        M_FATAL("Failed to create iocp handle: {}", SYSTEM_ERROR_CODE.value());
    }

    data_ = new Data{
        handle,

        (LPFN_ACCEPTEX)accept_ptr,
        (LPFN_CONNECTEX)connect_ptr,
        (LPFN_GETACCEPTEXSOCKADDRS)get_sock_addr_ptr
    };
}

IoCompletionPort::~IoCompletionPort() {
    if (data_) {
        ::CloseHandle(data_->handle);
        delete data_;
        data_ = nullptr;
    }
}

void IoCompletionPort::write_file(IoHandle ioh, size_t offset, IoContext *ioc) {
    ++data_->io_num;
    ZeroMemory(&ioc->overlapped, sizeof(OVERLAPPED));
    ioc->overlapped.Offset = offset;

    BOOL status = WriteFile(
        ioh.ptr, 
        ioc->iovec.buf, 
        ioc->iovec.len, 
        NULL, 
        &ioc->overlapped
    );
    
    if (!status && ERROR_IO_PENDING != ::GetLastError()) {
        ioc->cb(SYSTEM_ERROR_CODE, ioc, ioc->ptr);
    }
}

void IoCompletionPort::read_file(IoHandle ioh, size_t offset, IoContext *ioc) {
    ++data_->io_num;
    ZeroMemory(&ioc->overlapped, sizeof(OVERLAPPED));
    ioc->overlapped.Offset = offset;

    BOOL status = ReadFile(
        ioh.ptr, 
        ioc->iovec.buf, 
        ioc->iovec.len, 
        NULL, 
        &ioc->overlapped
    );
    
    if (!status && ERROR_IO_PENDING != ::GetLastError()) {
        ioc->cb(SYSTEM_ERROR_CODE, ioc, ioc->ptr);
    }
}

void IoCompletionPort::accept(const net::Socket& listener, IoContext *ioc) {
    ++data_->io_num;
    ZeroMemory(&ioc->overlapped, sizeof(OVERLAPPED));

    std::error_code ec;
    SOCKET sock_handle = detail::open_socket(listener.ip(), listener.transport(), ec);
    if (ec) {
        ioc->cb(ec, ioc, ioc->ptr);
        return;
    }
    ioc->res = sock_handle;
    ioc->iovec.buf = new char[128]{};
    ioc->iovec.len = 128;
    auto listener_h = listener.handle();
    std::memcpy(&ioc->iovec.buf[120], &listener_h, sizeof(listener_h));

    bool status = data_->accept(
        listener.handle(),
        ioc->res,
        ioc->iovec.buf,
        0,
        sizeof(sockaddr_in6) + 16, // ipv4 ipv6
        sizeof(sockaddr_in6) + 16,
        NULL,
        &ioc->overlapped
    );

    if (!status && ERROR_IO_PENDING != ::GetLastError()) {
        detail::close_socket(sock_handle);
        delete[] ioc->iovec.buf;
        ioc->cb(SYSTEM_ERROR_CODE, ioc, ioc->ptr);
    }
}

void IoCompletionPort::connect(SocketHandle socket, IoContext *ioc) {
    ++data_->io_num;
    ZeroMemory(&ioc->overlapped, sizeof(OVERLAPPED));
    ioc->res = socket;

    bool status = data_->connect(
        socket,
        (sockaddr*)&ioc->remote_addr,
        ioc->addr_len,
        NULL,
        0,
        NULL,
        (LPOVERLAPPED)&ioc->overlapped
    );

    if (!status && ERROR_IO_PENDING != ::GetLastError()) {
        ioc->cb(SYSTEM_ERROR_CODE, ioc, ioc->ptr);
    }
}

void IoCompletionPort::send(SocketHandle socket, IoContext *ioc) {
    ++data_->io_num;
    ZeroMemory(&ioc->overlapped, sizeof(OVERLAPPED));

    DWORD flag = 0;
    int status = ::WSASend(
        socket, 
        &ioc->iovec, 
        1, 
        NULL, 
        flag,
        (LPOVERLAPPED)&ioc->overlapped, 
        NULL
    );

    if (SOCKET_ERROR == status && ERROR_IO_PENDING != ::GetLastError()) {
        ioc->cb(SYSTEM_ERROR_CODE, ioc, ioc->ptr);
    }
}

void IoCompletionPort::receive(SocketHandle socket, IoContext *ioc) {
    ++data_->io_num;
    ZeroMemory(&ioc->overlapped, sizeof(OVERLAPPED));

    DWORD flag = 0;
    int status = ::WSARecv(
        socket, 
        (LPWSABUF)&ioc->iovec, 
        1, 
        NULL,
        &flag,
        (LPOVERLAPPED)&ioc->overlapped, 
        NULL
    );

    if (SOCKET_ERROR == status && ERROR_IO_PENDING != ::GetLastError()) {
        ioc->cb(SYSTEM_ERROR_CODE, ioc, ioc->ptr);
    }
}

void IoCompletionPort::send_to(SocketHandle socket, IoContext *ioc) {
    ++data_->io_num;
    ZeroMemory(&ioc->overlapped, sizeof(OVERLAPPED));

    DWORD flag = 0;
    int status = ::WSASendTo(
        socket,
        &ioc->iovec, 
        1, 
        NULL, 
        flag, 
        (const sockaddr *)&ioc->remote_addr, 
        ioc->addr_len, 
        (LPOVERLAPPED)&ioc->overlapped,
        NULL
    );

    if (SOCKET_ERROR == status && ERROR_IO_PENDING != ::GetLastError()) {
        ioc->cb(SYSTEM_ERROR_CODE, ioc, ioc->ptr);
    }
}

void IoCompletionPort::receive_from(SocketHandle socket, IoContext *ioc) {
    ++data_->io_num;
    ZeroMemory(&ioc->overlapped, sizeof(OVERLAPPED));

    DWORD flag = 0;
    int status = ::WSARecvFrom(
        socket, 
        &ioc->iovec, 
        1,
        NULL, 
        &flag,
        (sockaddr*)&ioc->remote_addr, 
        &ioc->addr_len, 
        (LPOVERLAPPED)&ioc->overlapped, 
        NULL
    );

    if (SOCKET_ERROR == status && ERROR_IO_PENDING != ::GetLastError()) {
        ioc->cb(SYSTEM_ERROR_CODE, ioc, ioc->ptr);
    }
}

void IoCompletionPort::cancel(IoHandle ioh) {
    ::CancelIoEx((HANDLE)ioh.ptr, NULL);
}

int IoCompletionPort::poll(size_t nanosec, std::error_code &ec) {
    if (nanosec == 0 && data_->io_num == 0) {
        return 0;
    }

    size_t wait_time = nanosec / 1000000;
    for (int i = 0; i < 1024; ++i) {
        std::error_code inner_ec;
        DWORD bytes_transferred = 0;
        IoContext* ioc = nullptr;
        void* key = nullptr;

        bool status = ::GetQueuedCompletionStatus(
            data_->handle,
            &bytes_transferred, 
            (PULONG_PTR)&key, 
            (LPOVERLAPPED*)&ioc,
            (ULONG)wait_time
        );
        wait_time = 0;

        if (!status) {
            if (WAIT_TIMEOUT == ::GetLastError()) {
                return 0;
            }
            inner_ec = SYSTEM_ERROR_CODE;
        }
        
        if (ULONG_MAX == bytes_transferred) {
            // be waked up
            continue;
        } else if (!ioc) {
            ec = SYSTEM_ERROR_CODE;
            return -1;
        }       

        --data_->io_num;
        switch(ioc->op) {
        case Operation::WriteFile:
        case Operation::ReadFile:
            ioc->res = bytes_transferred;
            break;
        case Operation::Accept: {
            if (!inner_ec) {
                LPSOCKADDR local_addr_ptr;
                LPSOCKADDR remote_addr_ptr;
                int local_addr_len = 0;
                int remote_addr_len = 0;

                data_->get_sock_addr(
                    ioc->iovec.buf,
                    0,
                    sizeof(sockaddr_in6) + 16,
                    sizeof(sockaddr_in6) + 16,
                    &local_addr_ptr,
                    &local_addr_len,
                    &remote_addr_ptr,
                    &remote_addr_len
                );

                std::memcpy(&ioc->remote_addr, remote_addr_ptr, remote_addr_len);
                ioc->addr_len = remote_addr_len;

                auto listener_h = *(SOCKET*)&ioc->iovec.buf[120];
                ::setsockopt(
                    ioc->res, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, 
                    (const char*)&listener_h, sizeof(listener_h)
                );
            } else {
                detail::close_socket(ioc->res);
            }
            delete[] ioc->iovec.buf;
        }
            break;
        case Operation::Connect: {
            if (!inner_ec) {
                ::setsockopt(
                    ioc->res, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT,
                    NULL, 0
                );
            }
        }
            break;
        case Operation::Send: 
        case Operation::Receive:
        case Operation::SendTo:
        case Operation::ReceiveFrom:
            ioc->res = bytes_transferred;
            break;
        default:
            break;
        }

        ioc->cb(inner_ec, ioc, ioc->ptr);
    }

    return 1;
}

void IoCompletionPort::attach(IoHandle ioh, std::error_code &ec) {
    HANDLE handle = ::CreateIoCompletionPort(
        ioh.ptr, 
        data_->handle, 
        NULL, 
        0
    );

    if (!handle) {
        ec = SYSTEM_ERROR_CODE;
    }
}

void IoCompletionPort::wake_up() {
    ::PostQueuedCompletionStatus(
        data_->handle, 
        ULONG_MAX,
        0, 
        NULL
    );
}

}

}


#endif