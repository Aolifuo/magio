#ifdef _WIN32
#include "magio-v3/net/iocp.h"

#include "magio-v3/core/error.h"
#include "magio-v3/core/logger.h"
#include "magio-v3/core/io_context.h"
#include "magio-v3/net/socket.h"

#include <MSWSock.h>
#include <Ws2tcpip.h> // for socklen_t

namespace magio {

namespace net {

struct IoCompletionPort::Data {
    HANDLE handle;

    LPFN_ACCEPTEX accept;
    LPFN_CONNECTEX connect;
    LPFN_GETACCEPTEXSOCKADDRS get_sock_addr;
};

IoCompletionPort::IoCompletionPort() {
    WSADATA wsa_data;
    if (0 != ::WSAStartup(MAKEWORD(2, 2), &wsa_data)) {
        M_FATAL("{}", "failed to start wsa");
    }

    std::error_code ec;
    Socket socket;
    socket.open(Ip::v4, Transport::Tcp, ec);
    if (ec) {
        M_FATAL("Failed to open a socket in iocp: {}", ec.value());
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
        M_FATAL("Failed to get accept func: {}", SOCKET_ERROR_CODE.value());
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
        M_FATAL("Failed to get connect func: {}", SOCKET_ERROR_CODE.value());
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
        M_FATAL("Failed to get sock addr func: {}", SOCKET_ERROR_CODE.value());
    }

    HANDLE handle = ::CreateIoCompletionPort(
        INVALID_HANDLE_VALUE, 
        NULL, 
        0, 
        1
    );

    if (!handle) {
        M_FATAL("Failed to create iocp handle: {}", SOCKET_ERROR_CODE.value());
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
        ::WSACleanup();
        ::CloseHandle(data_->handle);
        delete data_;
        data_ = nullptr;
    }
}

void IoCompletionPort::read_file(IoContext &ioc, size_t offset) {
    ZeroMemory(&ioc.overlapped, sizeof(OVERLAPPED));
    ioc.op = Operation::ReadFile;
    LARGE_INTEGER large;
    large.QuadPart = offset;
    ioc.overlapped.Offset = large.LowPart;
    ioc.overlapped.OffsetHigh = large.HighPart;

    BOOL status = ReadFile(
        (HANDLE)ioc.handle, 
        ioc.buf.buf, 
        ioc.buf.len, 
        NULL, 
        &ioc.overlapped
    );
    
    if (!status && ERROR_IO_PENDING != ::GetLastError()) {
        ioc.cb(SOCKET_ERROR_CODE, ioc.ptr);
    }
}

void IoCompletionPort::write_file(IoContext &ioc, size_t offset) {
    ZeroMemory(&ioc.overlapped, sizeof(OVERLAPPED));
    ioc.op = Operation::WriteFile;
    ioc.overlapped.Offset = ::SetFilePointer(
        (HANDLE)ioc.handle, 0, 
        NULL, FILE_END
    );

    BOOL status = WriteFile(
        (HANDLE)ioc.handle, 
        ioc.buf.buf, 
        ioc.buf.len, 
        NULL, 
        &ioc.overlapped
    );
    
    if (!status && ERROR_IO_PENDING != ::GetLastError()) {
        ioc.cb(SOCKET_ERROR_CODE, ioc.ptr);
    }
}

void IoCompletionPort::connect(IoContext& ioc) {
    ZeroMemory(&ioc.overlapped, sizeof(OVERLAPPED));
    ioc.op = Operation::Connect;

    bool status = data_->connect(
        ioc.handle,
        (sockaddr*)&ioc.remote_addr,
        ioc.addr_len,
        NULL,
        0,
        NULL,
        (LPOVERLAPPED)&ioc.overlapped
    );

    if (!status && ERROR_IO_PENDING != ::GetLastError()) {
        ioc.cb(SOCKET_ERROR_CODE, ioc.ptr);
    }
}

void IoCompletionPort::accept(Socket &listener, IoContext &ioc) {
    ZeroMemory(&ioc.overlapped, sizeof(OVERLAPPED));
    ioc.op = Operation::Accept;

    std::error_code ec;
    SOCKET sock_handle = detail::open_socket(
        listener.ip(), listener.transport(), ec);
    if (ec) {
        ioc.cb(ec, ioc.ptr);
        return;
    }
    ioc.handle = sock_handle;

    bool status = data_->accept(
        listener.handle(),
        ioc.handle,
        ioc.buf.buf,
        0,
        sizeof(sockaddr_in6) + 16, // ipv4 ipv6
        sizeof(sockaddr_in6) + 16,
        NULL,
        (LPOVERLAPPED)&ioc.overlapped
    );

    if (!status && ERROR_IO_PENDING != ::GetLastError()) {
        detail::close_socket(sock_handle);
        ioc.cb(SOCKET_ERROR_CODE, ioc.ptr);
        return;
    }
}

void IoCompletionPort::send(IoContext &ioc) {
    ZeroMemory(&ioc.overlapped, sizeof(OVERLAPPED));
    ioc.op = Operation::Send;

    DWORD flag = 0;
    int status = ::WSASend(
        ioc.handle, 
        &ioc.buf, 
        1, 
        NULL, 
        flag, 
        (LPOVERLAPPED)&ioc.overlapped, 
        NULL
    );

    if (SOCKET_ERROR == status && ERROR_IO_PENDING != ::GetLastError()) {
        ioc.cb(SOCKET_ERROR_CODE, ioc.ptr);
    }
}

void IoCompletionPort::receive(IoContext &ioc) {
    ZeroMemory(&ioc.overlapped, sizeof(OVERLAPPED));
    ioc.op = Operation::Receive;

    DWORD flag = 0;
    int status = ::WSARecv(
        ioc.handle, 
        (LPWSABUF)&ioc.buf, 
        1, 
        NULL,
        &flag,
        (LPOVERLAPPED)&ioc.overlapped, 
        NULL
    );

    if (SOCKET_ERROR == status && ERROR_IO_PENDING != ::GetLastError()) {
        ioc.cb(SOCKET_ERROR_CODE, ioc.ptr);
    }
}

void IoCompletionPort::send_to(IoContext &ioc) {
    ZeroMemory(&ioc.overlapped, sizeof(OVERLAPPED));
    ioc.op = Operation::Send;

    DWORD flag = 0;
    int status = ::WSASendTo(
        ioc.handle,
        &ioc.buf, 
        1, 
        NULL, 
        flag, 
        (const sockaddr *)&ioc.remote_addr, 
        ioc.addr_len, 
        (LPOVERLAPPED)&ioc.overlapped,
        NULL
    );

    if (SOCKET_ERROR == status && ERROR_IO_PENDING != ::GetLastError()) {
        ioc.cb(SOCKET_ERROR_CODE, ioc.ptr);
    }
}

void IoCompletionPort::receive_from(IoContext &ioc) {
    ZeroMemory(&ioc.overlapped, sizeof(OVERLAPPED));
    ioc.op = Operation::Receive;

    DWORD flag = 0;
    int status = ::WSARecvFrom(
        ioc.handle, 
        &ioc.buf, 
        1,
        NULL, 
        &flag,
        (sockaddr*)&ioc.remote_addr, 
        &ioc.addr_len, 
        (LPOVERLAPPED)&ioc.overlapped, 
        NULL
    );

    if (SOCKET_ERROR == status && ERROR_IO_PENDING != ::GetLastError()) {
        ioc.cb(SOCKET_ERROR_CODE, ioc.ptr);
    }
}

int IoCompletionPort::poll(bool block, std::error_code &ec) {
    DWORD bytes_transferred = 0;
    IoContext* ioc = nullptr;
    void* key = nullptr;
    std::error_code inner_ec;

    ULONG wait_time = block ? ULONG_MAX : 0;
    bool status = ::GetQueuedCompletionStatus(
        data_->handle,
        &bytes_transferred, 
        (PULONG_PTR)&key, 
        (LPOVERLAPPED*)&ioc,
        wait_time
    );

    if (!status) {
        if (WAIT_TIMEOUT == ::GetLastError()) {
            return 0;
        }
        inner_ec = SOCKET_ERROR_CODE;
    }

    if (!ioc) {
        ec = SOCKET_ERROR_CODE;
        return -1;
    }

    // wake up
    if (ULONG_MAX == bytes_transferred) {
        return 2;
    }

    // handle 
    switch(ioc->op) {
    case Operation::ReadFile: {
        ioc->buf.len = bytes_transferred;
    }
        break;
    case Operation::WriteFile: {
        ioc->buf.len = bytes_transferred;
    }
        break;
    case
     Operation::Accept: {
        LPSOCKADDR local_addr_ptr;
        LPSOCKADDR remote_addr_ptr;
        int local_addr_len = 0;
        int remote_addr_len = 0;

        data_->get_sock_addr(
            ioc->buf.buf,
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
    }
        break;
    case Operation::Connect: {
    }
        break;
    case Operation::Send: {
        ioc->buf.len = bytes_transferred;
    }
        break;
    case Operation::Receive: {
        ioc->buf.len = bytes_transferred;
    }
        break;
    }

    ioc->cb(inner_ec, ioc->ptr);
    return 1;
}

void IoCompletionPort::relate(void* sock_handle, std::error_code& ec) {
    HANDLE handle = ::CreateIoCompletionPort(
        (HANDLE)sock_handle, 
        data_->handle, 
        NULL, 
        0
    );

    if (!handle) {
        ec = SOCKET_ERROR_CODE;
    }
}

void IoCompletionPort::wake_up() {
    void* key = nullptr;
    ::PostQueuedCompletionStatus(
        data_->handle, 
        ULONG_MAX,
        (ULONG_PTR)key, 
        NULL
    );
}

}

}


#endif