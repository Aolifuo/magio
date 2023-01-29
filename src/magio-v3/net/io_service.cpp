#include "magio-v3/core/io_service.h"

#include "magio-v3/core/io_context.h"
#include "magio-v3/net/socket.h"
#include "magio-v3/net/address.h"

namespace magio {

void IoService::write_file(IoHandle ioh, const char *msg, size_t len, size_t offset, void *user_ptr, Cb cb) {
    auto ioc = new IoContext{
        .op = Operation::WriteFile,
        .iovec = io_buf((char*)msg, len),
        .ptr = user_ptr,
        .cb = cb
    };
    
    impl_->write_file(ioh, offset, ioc);
}

void IoService::read_file(IoHandle ioh, char* buf, size_t len, size_t offset, void *user_ptr, Cb cb) {
    auto ioc = new IoContext{
        .op = Operation::ReadFile,
        .iovec = io_buf(buf, len),
        .ptr = user_ptr,
        .cb = cb
    };

    impl_->read_file(ioh, offset, ioc);
}

void IoService::accept(const net::Socket& listener, void *user_ptr, Cb cb) {
    auto ioc = new IoContext{
        .op = Operation::Accept,
        .ptr = user_ptr,
        .cb = cb
    };
    
    impl_->accept(listener, ioc);
}

void IoService::connect(SocketHandle socket, const net::InetAddress &remote, void *user_ptr, Cb cb) {
    auto ioc = new IoContext{
        .op = Operation::Connect,
        .addr_len = (socklen_t)remote.sockaddr_len(),
        .ptr = user_ptr,
        .cb = cb
    };

    std::memcpy(&ioc->remote_addr, remote.buf_, ioc->addr_len);
    impl_->connect(socket, ioc);
}

void IoService::send(SocketHandle socket, const char *msg, size_t len, void *user_ptr, Cb cb) {
    auto ioc = new IoContext{
        .op = Operation::Send,
        .iovec = io_buf((char*)msg, len),
        .ptr = user_ptr,
        .cb = cb
    };

    impl_->send(socket, ioc);
}

void IoService::receive(SocketHandle socket, char *buf, size_t len, void *user_ptr, Cb cb) {
    auto ioc = new IoContext{
        .op = Operation::Send,
        .iovec = io_buf(buf, len),
        .ptr = user_ptr,
        .cb = cb
    };

    impl_->receive(socket, ioc);
}

void IoService::send_to(SocketHandle socket, const net::InetAddress &remote, const char *msg, size_t len, void *user_ptr, Cb cb) {
    auto ioc = new IoContext{
        .op = Operation::SendTo,
        .iovec = io_buf((char*)msg, len),
        .addr_len = (socklen_t)remote.sockaddr_len(),
        .ptr = user_ptr,
        .cb = cb
    };
    std::memcpy(&ioc->remote_addr, remote.buf_, ioc->addr_len);

#ifdef __linux__
    auto info = msghdr{
        .msg_name = &ioc->remote_addr,
        .msg_namelen = ioc->addr_len,
        .msg_iov = (iovec*)&ioc->iovec,
        .msg_iovlen = 1,
        .msg_control = nullptr,
        .msg_controllen = 0,
        .msg_flags = 0
    };

    ioc->ptr = new ResumeWithMsg{
        .msg = {info},
        .ptr = user_ptr
    };
#endif

    impl_->send_to(socket, ioc);
}

void IoService::receive_from(SocketHandle socket, char *buf, size_t len, void *user_ptr, Cb cb) {
    auto ioc = new IoContext{
        .op = Operation::ReceiveFrom,
        .iovec = io_buf(buf, len),
        .addr_len = sizeof(sockaddr_in6),
        .ptr = user_ptr,
        .cb = cb
    };

#ifdef __linux__
    auto info = msghdr{
        .msg_name = &ioc->remote_addr,
        .msg_namelen = ioc->addr_len,
        .msg_iov = (iovec*)&ioc->iovec,
        .msg_iovlen = 1,
        .msg_control = nullptr,
        .msg_controllen = 0,
        .msg_flags = 0
    };

    ioc->ptr = new ResumeWithMsg{
        .msg = {info},
        .ptr = user_ptr
    };
#endif

    impl_->receive_from(socket, ioc);
}

void IoService::cancel(IoHandle ioh) {
    impl_->cancel(ioh);
}

void IoService::attach(IoHandle ioh, std::error_code &ec) {
    impl_->attach(ioh, ec);
}

int IoService::poll(size_t nanosec, std::error_code &ec) {
    return impl_->poll(nanosec, ec);
}

void IoService::wake_up() {
    impl_->wake_up();
}

}