#ifdef __linux__
#include "magio-v3/net/io_uring.h"

#include "magio-v3/core/logger.h"
#include "magio-v3/core/error.h"
#include "magio-v3/core/io_context.h"
#include "magio-v3/net/socket.h"

#include <netinet/in.h>

#include "liburing.h"

namespace magio {

namespace net {

IoUring::IoUring(unsigned entries) {
    io_uring_params params;
    std::memset(&params, 0, sizeof(params));

    p_io_uring_ = new io_uring;
    if (0 > ::io_uring_queue_init_params(entries, p_io_uring_, &params)) {
        delete p_io_uring_;
        M_FATAL("failed to create io uring: {}", SOCKET_ERROR_CODE.message());
    }
}

IoUring::~IoUring() {
    if (p_io_uring_) {
        ::io_uring_queue_exit(p_io_uring_);
        delete p_io_uring_;
        p_io_uring_ = nullptr;
    }
}

void IoUring::read_file(IoContext &ioc) {
    ioc.op = Operation::ReadFile;
    io_uring_sqe* sqe = ::io_uring_get_sqe(p_io_uring_);
    ::io_uring_prep_read(sqe, ioc.handle, ioc.buf.buf, ioc.buf.len, 0);
    ::io_uring_sqe_set_data(sqe, &ioc);
    ::io_uring_submit(p_io_uring_);
}

void IoUring::write_file(IoContext &ioc) {
    ioc.op = Operation::WriteFile;
    io_uring_sqe* sqe = ::io_uring_get_sqe(p_io_uring_);
    ::io_uring_prep_write(sqe, ioc.handle, ioc.buf.buf, ioc.buf.len, 0);
    ::io_uring_sqe_set_data(sqe, &ioc);
    ::io_uring_submit(p_io_uring_);
}

void IoUring::connect(IoContext &ioc) {
    ioc.op = Operation::Connect;
    io_uring_sqe* sqe = ::io_uring_get_sqe(p_io_uring_);
    ::io_uring_prep_connect(
        sqe, ioc.handle, (sockaddr*)&ioc.remote_addr, 
        ioc.remote_addr.sin_family == AF_INET ? sizeof(sockaddr_in) : sizeof(sockaddr_in6)
    );
    ::io_uring_sqe_set_data(sqe, &ioc);
    ::io_uring_submit(p_io_uring_);
}

void IoUring::accept(Socket &listener, IoContext &ioc) {
    ioc.op = Operation::Accept;
    io_uring_sqe* sqe = ::io_uring_get_sqe(p_io_uring_);
    ::io_uring_prep_accept(
        sqe, listener.handle(), (sockaddr*)&ioc.remote_addr, 
        (socklen_t*)ioc.buf.buf, 0
    );
    ::io_uring_sqe_set_data(sqe, &ioc);
    ::io_uring_submit(p_io_uring_);
}

void IoUring::send(IoContext &ioc) {
    ioc.op = Operation::Send;
    io_uring_sqe* sqe = ::io_uring_get_sqe(p_io_uring_);
    ::io_uring_prep_send(sqe, ioc.handle, ioc.buf.buf, ioc.buf.len, 0);
    ::io_uring_sqe_set_data(sqe, &ioc);
    ::io_uring_submit(p_io_uring_);
}

void IoUring::receive(IoContext &ioc) {
    ioc.op = Operation::Receive;
    io_uring_sqe* sqe = ::io_uring_get_sqe(p_io_uring_);
    ::io_uring_prep_recv(sqe, ioc.handle, ioc.buf.buf, ioc.buf.len, 0);
    ::io_uring_sqe_set_data(sqe, &ioc);
    ::io_uring_submit(p_io_uring_);
}

void IoUring::send_to(IoContext &ioc) {

}

void IoUring::receive_from(IoContext &ioc) {

}

int IoUring::poll(bool block, std::error_code &ec) {
    std::error_code inner_ec;
    io_uring_cqe* cqe = nullptr;
    int r;

    r = ::io_uring_wait_cqe_nr(p_io_uring_, &cqe, block);
    if (-EAGAIN == r) {
        return 0;
    } else if (0 != r) {
        ec = make_socket_error_code(-r);
        M_SYS_ERROR("io_uring_wait_cqe error: {}", ec.message());
        return -1;
    }
    
    IoContext* ioc = (IoContext*)::io_uring_cqe_get_data(cqe);
    if (cqe->res < 0) {
        inner_ec = make_socket_error_code(-cqe->res);
    }
    switch (ioc->op) {
    case Operation::ReadFile: {
        int bytes_read = cqe->res;
        ioc->buf.len = bytes_read;
    }
        break;
    case Operation::WriteFile: {
        int bytes_write = cqe->res;
        ioc->buf.len = bytes_write;
    }
        break;
    case Operation::Accept: {
        ioc->handle = cqe->res;
        ::getpeername(
            ioc->handle, 
            (sockaddr*)&ioc->remote_addr,
            (socklen_t*)ioc->buf.buf
        );
    }
        break;
    case Operation::Connect: {
    }
        break;
    case Operation::Receive: {
        int bytes_read = cqe->res;
        ioc->buf.len = bytes_read;
    }
        break;
    case Operation::Send: {
        int bytes_write = cqe->res;
        ioc->buf.len = bytes_write;
    }
        break;
    }

    ::io_uring_cqe_seen(p_io_uring_, cqe);
    ioc->cb(inner_ec, ioc->ptr);
    return 1;
}

void IoUring::relate(void *sock_handle, std::error_code &ec) {
    return;
}

void IoUring::wake_up() { // ?

}

}

}

#endif