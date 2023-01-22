#ifdef __linux__
#include "magio-v3/net/io_uring.h"

#include "magio-v3/core/logger.h"
#include "magio-v3/core/error.h"
#include "magio-v3/core/io_context.h"
#include "magio-v3/net/socket.h"

#include <unistd.h>
#include <sys/eventfd.h>

#include "liburing.h"

namespace magio {

namespace net {

IoUring::IoUring(unsigned entries) {
    io_uring_params params;
    std::memset(&params, 0, sizeof(params));

    p_io_uring_ = new io_uring;
    if (0 > ::io_uring_queue_init_params(entries, p_io_uring_, &params)) {
        delete p_io_uring_;
        M_FATAL("failed to create io uring: {}", SYSTEM_ERROR_CODE.message());
    }

    wake_up_fd_ = ::eventfd(EFD_NONBLOCK, 0);
    wake_up_ctx_ = new WakeupContext{
        .op = Operation::Noop,
        .ptr = this,
        .cb = [](std::error_code ec, WakeupContext* ctx) {
            auto ioring = (IoUring*)ctx->ptr;
            ioring->prep_wake_up();
        }
    };

    prep_wake_up();
}

IoUring::~IoUring() {
    if (p_io_uring_) {
        ::close(wake_up_fd_);
        ::io_uring_queue_exit(p_io_uring_);
        delete wake_up_ctx_;
        delete p_io_uring_;
    }
}

void IoUring::write_file(IoHandle ioh, size_t offset, IoContext *ioc) {
    ++io_num_;
    io_uring_sqe* sqe = ::io_uring_get_sqe(p_io_uring_);
    ::io_uring_prep_write(sqe, ioh.a, ioc->iovec.buf, ioc->iovec.len, offset);
    ::io_uring_sqe_set_data(sqe, ioc);
}

void IoUring::read_file(IoHandle ioh, size_t offset, IoContext *ioc){
    ++io_num_;
    io_uring_sqe* sqe = ::io_uring_get_sqe(p_io_uring_);
    ::io_uring_prep_read(sqe, ioh.a, ioc->iovec.buf, ioc->iovec.len, offset);
    ::io_uring_sqe_set_data(sqe, ioc);
}

void IoUring::accept(const net::Socket &listener, IoContext *ioc) {
    ++io_num_;
    io_uring_sqe* sqe = ::io_uring_get_sqe(p_io_uring_);
    ::io_uring_prep_accept(
        sqe, listener.handle(), (sockaddr*)&ioc->remote_addr, 
        &ioc->addr_len, 0
    );
    ::io_uring_sqe_set_data(sqe, ioc);
}

void IoUring::connect(SocketHandle socket, IoContext *ioc) {
    ++io_num_;
    io_uring_sqe* sqe = ::io_uring_get_sqe(p_io_uring_);
    ::io_uring_prep_connect(
        sqe, socket, (sockaddr*)&ioc->remote_addr, ioc->addr_len
    );
    ::io_uring_sqe_set_data(sqe, ioc);
}

void IoUring::send(SocketHandle socket, IoContext *ioc) {
    ++io_num_;
    io_uring_sqe* sqe = ::io_uring_get_sqe(p_io_uring_);
    ::io_uring_prep_send(sqe, socket, ioc->iovec.buf, ioc->iovec.len, 0);
    ::io_uring_sqe_set_data(sqe, ioc);
}

void IoUring::receive(SocketHandle socket, IoContext *ioc) {
    ++io_num_;
    io_uring_sqe* sqe = ::io_uring_get_sqe(p_io_uring_);
    ::io_uring_prep_recv(sqe, socket, ioc->iovec.buf, ioc->iovec.len, 0);
    ::io_uring_sqe_set_data(sqe, ioc);
}

void IoUring::send_to(SocketHandle socket, IoContext *ioc) {
    ++io_num_;
    io_uring_sqe* sqe = ::io_uring_get_sqe(p_io_uring_);
    auto rwm = (ResumeWithMsg*)ioc->ptr;
    ::io_uring_prep_sendmsg(sqe, socket, &rwm->msg, 0);
    ::io_uring_sqe_set_data(sqe, ioc);
}

void IoUring::receive_from(SocketHandle socket, IoContext *ioc) {
    ++io_num_;
    io_uring_sqe* sqe = ::io_uring_get_sqe(p_io_uring_);
    auto rwm = (ResumeWithMsg*)ioc->ptr;
    ::io_uring_prep_recvmsg(sqe, socket, &rwm->msg, 0);
    ::io_uring_sqe_set_data(sqe, ioc);
}

// invoke all completion
int IoUring::poll(size_t nanosec, std::error_code &ec) {
    if (nanosec == 0 && io_num_ == 0) {
        return 0;
    }
    ::io_uring_submit(p_io_uring_);

    io_uring_cqe* cqe;
    int r = ::io_uring_peek_cqe(p_io_uring_, &cqe);
    if (-EAGAIN == r) {
        // nothing then sleep
        prep_timeout(nanosec);
        r = ::io_uring_submit_and_wait(p_io_uring_, 1);
        if (-EAGAIN == r) {
            return 0;
        } else if (-EINTR == r) {
            return 2;
        } else if (r < 0) {
            ec = make_socket_error_code(-r);
            return -1;
        }
    } else if (-EINTR == r) {
        return 2;
    } else if (r < 0) {
        ec = make_socket_error_code(-r);
        return -1;
    }
    
    unsigned head, count = 0;
    io_uring_for_each_cqe(p_io_uring_, head, cqe) {
        ++count;
        handle_cqe(cqe);
    }
    ::io_uring_cq_advance(p_io_uring_, count);
    
    return 1;
}

void IoUring::handle_cqe(io_uring_cqe* cqe) {
    std::error_code inner_ec;
    IoContext* ioc = (IoContext*)::io_uring_cqe_get_data(cqe);

    if (ioc->op != Operation::Noop) {
        --io_num_;
    } else {
        auto wctx = (WakeupContext*)ioc;
        wctx->cb(cqe->res < 0 ? make_socket_error_code(-cqe->res) : std::error_code{}, wctx);
        return;
    }

    if (cqe->res < 0) {
        inner_ec = make_socket_error_code(-cqe->res);
        ioc->res = 0;
    } else {
        ioc->res = cqe->res;
        switch (ioc->op) {
        case Operation::WriteFile:
        case Operation::ReadFile:
            break;
        case Operation::Accept: {
            ::getpeername((int)ioc->res, (sockaddr*)&ioc->remote_addr, &ioc->addr_len);
        }
            break;
        case Operation::Connect:
        case Operation::Send:
        case Operation::Receive:
            break;
        case Operation::SendTo:
        case Operation::ReceiveFrom: {
            auto rwm = (ResumeWithMsg*)ioc->ptr;
            ioc->addr_len = rwm->msg.msg_namelen;
            ioc->ptr = rwm->ptr;
            delete rwm;
        }
            break;
        default:
            break;
        }
    }

    ioc->cb(inner_ec, ioc, ioc->ptr);
}

void IoUring::wake_up() {
    uint64_t data{114514};
    ::write(wake_up_fd_, &data, sizeof(uint64_t));
}

void IoUring::prep_timeout(size_t nanosec) {
    auto ctx = new WakeupContext{};
    ctx->op = Operation::Noop;
    ctx->ts.tv_sec = (long long)(nanosec / 1000000000);
    ctx->ts.tv_nsec = (long long)(nanosec % 1000000000);
    ctx->cb = [](std::error_code ec, WakeupContext* ctx) {
        delete ctx;
    };
    
    io_uring_sqe* sqe = ::io_uring_get_sqe(p_io_uring_);
    ::io_uring_prep_timeout(sqe, &ctx->ts, 1, 0);
    ::io_uring_sqe_set_data(sqe, ctx);
}

void IoUring::prep_wake_up() {
    io_uring_sqe* sqe = ::io_uring_get_sqe(p_io_uring_);
    ::io_uring_prep_read(sqe, wake_up_fd_, &wake_up_ctx_->res, sizeof(uint64_t), 0);
    ::io_uring_sqe_set_data(sqe, wake_up_ctx_);
}

void IoUring::attach(IoHandle ioh, std::error_code &ec) {
    return;
}

}

}

#endif