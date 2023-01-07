#ifdef __linux__
#include "magio-v3/net/io_uring.h"

#include "magio-v3/core/logger.h"
#include "magio-v3/core/error.h"
#include "magio-v3/core/io_context.h"
#include "magio-v3/net/socket.h"

#include <unistd.h>
#include <netinet/in.h>
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

    wake_up_ctx_ = new IoContext;
    wake_up_ctx_->op = Operation::WakeUp;
    wake_up_ctx_->handle = ::eventfd(EFD_NONBLOCK, 0);
    wake_up_ctx_->ptr = (void*)1;
    wake_up_ctx_->cb = [](std::error_code ec, IoContext* ioc, void* p) {
        if (ec) {
            M_SYS_ERROR("wake up error: {}", ec.message());
        }
    };
    std::memset(&wake_up_ctx_->remote_addr, 1, sizeof(void*));

    prep_wake_up();
}

IoUring::~IoUring() {
    if (p_io_uring_) {
        ::close(wake_up_ctx_->handle);
        ::io_uring_queue_exit(p_io_uring_);
        delete wake_up_ctx_;
        delete p_io_uring_;
        wake_up_ctx_ = nullptr;
        p_io_uring_ = nullptr;
    }
}

void IoUring::read_file(IoContext &ioc, size_t offset) {
    ++io_num_;
    ioc.op = Operation::ReadFile;
    io_uring_sqe* sqe = ::io_uring_get_sqe(p_io_uring_);
    ::io_uring_prep_read(sqe, ioc.handle, ioc.buf.buf, ioc.buf.len, offset);
    ::io_uring_sqe_set_data(sqe, &ioc);
}

void IoUring::write_file(IoContext &ioc, size_t offset) {
    ++io_num_;
    ioc.op = Operation::WriteFile;
    io_uring_sqe* sqe = ::io_uring_get_sqe(p_io_uring_);
    ::io_uring_prep_write(sqe, ioc.handle, ioc.buf.buf, ioc.buf.len, offset);
    ::io_uring_sqe_set_data(sqe, &ioc);
}

void IoUring::connect(IoContext &ioc) {
    ++io_num_;
    ioc.op = Operation::Connect;
    io_uring_sqe* sqe = ::io_uring_get_sqe(p_io_uring_);
    ::io_uring_prep_connect(
        sqe, ioc.handle, (sockaddr*)&ioc.remote_addr, ioc.addr_len
    );
    ::io_uring_sqe_set_data(sqe, &ioc);
}

void IoUring::accept(Socket &listener, IoContext &ioc) {
    ++io_num_;
    ioc.op = Operation::Accept;
    io_uring_sqe* sqe = ::io_uring_get_sqe(p_io_uring_);
    ::io_uring_prep_accept(
        sqe, listener.handle(), (sockaddr*)&ioc.remote_addr, 
        (socklen_t*)ioc.buf.buf, 0
    );
    ::io_uring_sqe_set_data(sqe, &ioc);
}

void IoUring::send(IoContext &ioc) {
    ++io_num_;
    ioc.op = Operation::Send;
    io_uring_sqe* sqe = ::io_uring_get_sqe(p_io_uring_);
    ::io_uring_prep_send(sqe, ioc.handle, ioc.buf.buf, ioc.buf.len, 0);
    ::io_uring_sqe_set_data(sqe, &ioc);
}

void IoUring::receive(IoContext &ioc) {
    ++io_num_;
    ioc.op = Operation::Receive;
    io_uring_sqe* sqe = ::io_uring_get_sqe(p_io_uring_);
    ::io_uring_prep_recv(sqe, ioc.handle, ioc.buf.buf, ioc.buf.len, 0);
    ::io_uring_sqe_set_data(sqe, &ioc);
}

void IoUring::send_to(IoContext &ioc) {
    ++io_num_;
    ioc.op = Operation::Send;
    io_uring_sqe* sqe = ::io_uring_get_sqe(p_io_uring_);
    auto p = (ResumeWithMsg*)ioc.ptr;
    ::io_uring_prep_sendmsg(sqe, ioc.handle, &p->msg, 0);
    ::io_uring_sqe_set_data(sqe, &ioc);
}

void IoUring::receive_from(IoContext &ioc) {
    ++io_num_;
    ioc.op = Operation::Receive;
    io_uring_sqe* sqe = ::io_uring_get_sqe(p_io_uring_);
    auto p = (ResumeWithMsg*)ioc.ptr;
    ::io_uring_prep_recvmsg(sqe, ioc.handle, &p->msg, 0);
    ::io_uring_sqe_set_data(sqe, &ioc);
}

void IoUring::cancel(IoContext& ioc) {
    
}

// invoke all completion
int IoUring::poll(bool block, std::error_code &ec) {
    if (!block && io_num_ == 0) {
        return 0;
    }

    int r;

    r = ::io_uring_submit_and_wait(p_io_uring_, block);
    if (-EAGAIN == r) {
        // non block and no completion
        return 0;
    } else if (-EINTR == r) {
        return 2;
    } else if (r < 0) {
        ec = make_socket_error_code(-r);
        return -1;
    }

    unsigned count = ::io_uring_peek_batch_cqe(p_io_uring_, cqes_, sizeof(cqes_));
    for (unsigned i = 0; i < count; ++i) {
        std::error_code inner_ec;
        void* data = ::io_uring_cqe_get_data(cqes_[i]);
        IoContext* ioc = (IoContext*)data;

        if (ioc->op != Operation::WakeUp) {
            --io_num_;
        }
        
        if (cqes_[i]->res < 0) {
            inner_ec = make_socket_error_code(-cqes_[i]->res);
            ioc->buf.len = 0;
        } else {
            switch (ioc->op) {
            case Operation::WakeUp: {
                // be waked up
                prep_wake_up();
            }
                break;
            case Operation::ReadFile: {
                ioc->buf.len = cqes_[i]->res;
            }
                break;
            case Operation::WriteFile: {
                ioc->buf.len = cqes_[i]->res;
            }
                break;
            case Operation::Accept: {
                ioc->handle = cqes_[i]->res;
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
                ioc->buf.len = cqes_[i]->res;
            }
                break;
            case Operation::Send: {
                ioc->buf.len = cqes_[i]->res;
            }
                break;
            }
        }
        ioc->cb(inner_ec, ioc, ioc->ptr);
    }
    ::io_uring_cq_advance(p_io_uring_, count);

    return 1;
}

void IoUring::wake_up() {
    ::write(wake_up_ctx_->handle, &wake_up_ctx_->remote_addr, sizeof(void*));
}

void IoUring::prep_wake_up() {
    io_uring_sqe* sqe = ::io_uring_get_sqe(p_io_uring_);
    ::io_uring_prep_read(sqe, wake_up_ctx_->handle, &wake_up_ctx_->ptr, sizeof(void*), 0);
    ::io_uring_sqe_set_data(sqe, wake_up_ctx_);
}

void IoUring::relate(void *sock_handle, std::error_code &ec) {
    return;
}

}

}

#endif