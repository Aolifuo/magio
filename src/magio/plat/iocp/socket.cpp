#ifdef _WIN32

#include "magio/plat/iocp/socket.h"

#include <WinSock2.h>
#include <Ws2tcpip.h>

#include "magio/Configs.h"
#include "magio/plat/declare.h"

namespace magio {

namespace plat {

WSADATA WSA_DATA;

struct IOContext {
    OVERLAPPED overlapped;
    Socket* socket;
    WSABUF wsa_buf;
    char buf[global_config.buffer_size]{0};
    unsigned len = 0;
    IOOperation io_type = IOOperation::Noop;
};

struct Socket {
    SOCKET handle = INVALID_SOCKET;
    SOCKADDR_IN address;
    Address local_addr;
    Address remote_addr;
    IOContext io_recv;
    IOContext io_send;

    void init_io(IOContext& ioc) {
        ZeroMemory(&ioc.overlapped, sizeof(OVERLAPPED));
        ioc.wsa_buf.buf = ioc.buf;
        ioc.wsa_buf.len = global_config.buffer_size;
        ioc.socket = this;
    }

    Socket() {
        init_io(io_recv);
        init_io(io_send);
    }

    ~Socket() {
        if (handle != INVALID_SOCKET) {
            ::closesocket(handle);
        }
    }
};

// IOContextHelper

IOContextHelper::IOContextHelper(IOContext* ioc) 
    : ioc_(ioc) 
{}

void* IOContextHelper::overlapped() { 
    return &ioc_->overlapped; 
}

SocketHelper IOContextHelper::owner() { 
    return {ioc_->socket}; 
}

void* IOContextHelper::wsa_buf() { 
    return &ioc_->wsa_buf; 
}

char* IOContextHelper::buf() { 
    return ioc_->buf; 
}

unsigned IOContextHelper::capacity() { 
    return global_config.buffer_size; 
}

unsigned IOContextHelper::len() { 
    return ioc_->len; 
}

IOOperation IOContextHelper::io_operation() { 
    return ioc_->io_type; 
}

void IOContextHelper::set_io_operation(IOOperation io_type) {
    ioc_->io_type = io_type;
}

void IOContextHelper::set_len(unsigned len) { 
    ioc_->len = len; 
}

void IOContextHelper::reset_overlapped() {
    ZeroMemory(&ioc_->overlapped, sizeof(OVERLAPPED));
}

// SocketHelper
SocketHelper::SocketHelper(Socket* sock) 
    : sock_(sock) {}

SOCKET SocketHelper::handle() { 
    return sock_->handle; 
}

void* SocketHelper::address() { 
    return (void*)&sock_->address; 
}

Address& SocketHelper::local_addr() {
    return sock_->local_addr;
}

Address& SocketHelper::remote_addr() {
    return sock_->remote_addr;
}

IOContextHelper SocketHelper::recv_io() { 
    return {&sock_->io_recv}; 
}

IOContextHelper SocketHelper::send_io() { 
    return {&sock_->io_send}; 
}

Expected<> SocketHelper::bind(const char* host, short port) {
    sock_->address.sin_family = AF_INET;
    sock_->address.sin_port = ::htons(port);
    ::inet_pton(AF_INET, host, &sock_->address.sin_addr.S_un.S_addr);

    if (0 != ::bind(sock_->handle, (LPSOCKADDR)&sock_->address,
                    sizeof(SOCKADDR_IN))) {
        return {Error(last_error(), "Socket binding error")};
    }

    return {Unit()};
}

Expected<> SocketHelper::listen() {
    if (0 != ::listen(sock_->handle, SOMAXCONN)) {
        return {Error(last_error(), "Socket listening error")};
    }

    return {Unit()};
}

void SocketHelper::set_handle(SOCKET h) { 
    sock_->handle = h; 
}

void SocketHelper::set_address(void* addr) {
    sock_->address = *(LPSOCKADDR_IN)(addr);
}

void SocketHelper::set_recv_len(unsigned len) { 
    sock_->io_recv.len = len; 
}

void SocketHelper::set_send_len(unsigned len) { 
    sock_->io_send.len = len; 
}

void SocketHelper::for_async_task(IOOperation op) {
    switch (op) {
        case IOOperation::Accept:
            sock_->io_recv.io_type = IOOperation::Accept;
            ZeroMemory(&sock_->io_recv.overlapped, sizeof(OVERLAPPED));
            break;
        case IOOperation::Receive:
            sock_->io_recv.io_type = IOOperation::Receive;
            ZeroMemory(&sock_->io_recv.overlapped, sizeof(OVERLAPPED));
            sock_->io_recv.wsa_buf.len = global_config.buffer_size;
            break;
        case IOOperation::Send:
            sock_->io_send.io_type = IOOperation::Send;
            ZeroMemory(&sock_->io_send.overlapped, sizeof(OVERLAPPED));
            sock_->io_send.wsa_buf.len = sock_->io_send.len;
            break;
        case IOOperation::Connect:
            sock_->io_send.io_type = IOOperation::Connect;
            ZeroMemory(&sock_->io_send.overlapped, sizeof(OVERLAPPED));
            break;
        default:
            break;
    }
}

// SocketServer

struct SocketServer::Data {
    pool_type socket_pool;
    //std::vector<std::jthread> threads;

    Data(size_t socket_num)
        : socket_pool(socket_num, [](Socket* s) {
              if (INVALID_SOCKET != s->handle) {
                  ::closesocket(s->handle);
                  s->handle = INVALID_SOCKET;
              }
          }) 
    { }
};

SocketServer::SocketServer() { 
    data = new Data{global_config.max_sockets}; 
}

SocketServer::~SocketServer() {
    if (data) {
        close();
        delete data;
    }
}

Expected<> SocketServer::initialize() {
    if (0 != ::WSAStartup(MAKEWORD(2, 2), &WSA_DATA)) {
        return {Error("Failed to start socket server")};
    }

    return {Unit()};
}

SocketServer& SocketServer::instance() {
    static SocketServer server;
    return server;
}

void SocketServer::close() { 
    ::WSACleanup(); 
}

Expected<> get_socket(
    Socket* sock, TransportProtocol protocol = TransportProtocol::TCP) {
    switch (protocol) {
        case TransportProtocol::TCP:
            sock->handle = ::WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, 0, 0, WSA_FLAG_OVERLAPPED);
            break;
        case TransportProtocol::UDP:
            sock->handle = ::WSASocketW(AF_INET, SOCK_DGRAM, IPPROTO_UDP, 0, 0, WSA_FLAG_OVERLAPPED);
            break;
        default:
            return {Error("Unknown transport protocol")};
    }

    if (INVALID_SOCKET == sock->handle) {
        return {Error(last_error(), "Get a Invalid socket")};
    }

    return {Unit()};
}

Expected<Borrowed<SocketServer::pool_type>> SocketServer::make_socket(
    TransportProtocol protocol) {
    auto maybe_raw = raw_socket();
    if (!maybe_raw) {
        return maybe_raw.unwrap_err();
    }
    auto raw = maybe_raw.unwrap();

    if (auto res = get_socket(raw.get()); !res) {
        return res.unwrap_err();
    }

    return {std::move(raw)};
}

Expected<Borrowed<SocketServer::pool_type>> SocketServer::raw_socket() {
    auto maybe_borrowed = data->socket_pool.try_borrow();
    if (!maybe_borrowed) {
        return {Error("No free socket")};
    }
    auto socket = maybe_borrowed.unwrap();

    return {std::move(socket)};
}

Expected<> SocketServer::replace_socket(Socket* old, TransportProtocol protocol) {
    std::printf("old socket: %lld\n", old->handle);
    closesocket(old->handle);
    return get_socket(old);
}

SocketServer::pool_type* SocketServer::pool() { return &data->socket_pool; }

int last_error() { return ::GetLastError(); }

}  // namespace plat

}  // namespace magio
#endif