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
    wake_up_ctx_ = new IoContext{
        .op = Operation::Noop,
        .ptr = this,
        .cb = [](std::error_code ec, IoContext* ctx, void* ptr) {
            auto ioring = (IoUring*)ctx->ptr;
            ioring->prep_wake_up();
        }
    };

    empty_ctx_ = new IoContext{
        .op = Operation::Noop,
        .ptr = this,
        .cb = [](std::error_code ec, IoContext* ctx, void* ptr) {
            //
        }
    };

    prep_wake_up();
}

IoUring::~IoUring() {
    if (p_io_uring_) {
        ::close(wake_up_fd_);
        ::io_uring_queue_exit(p_io_uring_);
        delete wake_up_ctx_;
        delete empty_ctx_;
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
    printf("recv fd %d\n", socket);
    ::io_uring_prep_recvmsg(sqe, socket, &rwm->msg, 0);
    ::io_uring_sqe_set_data(sqe, ioc);
}

void IoUring::cancel(IoHandle ioh) {
    io_uring_sqe* sqe = ::io_uring_get_sqe(p_io_uring_);
    ::io_uring_prep_cancel_fd(sqe, ioh.a, 0);
    ::io_uring_sqe_set_data(sqe, empty_ctx_);
}

// invoke all completion
int IoUring::poll(size_t nanosec, std::error_code &ec) {
    if (nanosec == 0 && io_num_ == 0) {
        return 0;
    }
    __kernel_timespec ts{
        .tv_sec = (long long)(nanosec / 1000000000),
        .tv_nsec = (long long)(nanosec % 1000000000)
    };
    io_uring_cqe* cqe = nullptr;
    // ::io_uring_submit(p_io_uring_);

    int r = ::io_uring_submit_and_wait_timeout(p_io_uring_, &cqe, 1, &ts, nullptr);
    printf("r = %d naosec = %zu\n", r, nanosec);
    if (-ETIME == r || -EAGAIN == r) {
        return 0;
    } else if (-EINTR == r) {
        return 2;
    } else if (r < 0) {
        ec = make_system_error_code(-r);
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
    }

    if (cqe->res < 0) {
        inner_ec = make_system_error_code(-cqe->res);
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