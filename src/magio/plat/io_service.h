#pragma once

#ifdef __linux__
#include <netinet/in.h>
#include "liburing.h"
#endif

#ifdef _WIN32
#include "magio/plat/iocp/iocp.h"
#include <MSWSock.h>
#endif

#include "magio/core/Queue.h"
#include "magio/plat/socket.h"
#include "magio/utils/ScopeGuard.h"

namespace magio {

namespace detail {

inline socklen_t sockaddr_in_len = sizeof(sockaddr_in);

}

namespace plat {

#ifdef __linux__

class IOService {
public:
    IOService() = default;
    IOService(std::atomic_size_t& c)
        : count(&c)
    { }

    IOService(const IOService&) = delete;

    std::error_code open() {
        io_uring_params params;
        std::memset(&params, 0, sizeof(params));

        if (0 > ::io_uring_queue_init_params(MAGIO_LIBURING_ENTRIES, &ring_, &params)) {
            return MAGIO_SYSTEM_ERROR;
        }

        return {};
    }

    void close() {
        ::io_uring_queue_exit(&ring_);
    }

    void async_accept(int fd, IOData* io) {
        io->op = IOOP::Accept;
        io_uring_sqe* sqe = ::io_uring_get_sqe(&ring_);
        ::io_uring_prep_accept(sqe, fd, (sockaddr*)&io->remote, &detail::sockaddr_in_len, 0);
        set_sqe_data_and_flag(sqe, io, 0);
    }

    void async_connect(IOData* io, const sockaddr_in& addr) {
        io->op = IOOP::Connect;
        io_uring_sqe* sqe = ::io_uring_get_sqe(&ring_);
        ::io_uring_prep_connect(sqe, io->fd, (const sockaddr *)&addr, sizeof(addr));
        set_sqe_data_and_flag(sqe, io, 0);
    }

    void async_receive(IOData* io) {
        io->op = IOOP::Receive;
        io_uring_sqe* sqe = ::io_uring_get_sqe(&ring_);
        ::io_uring_prep_recv(sqe, io->fd, io->wsa_buf.buf, io->wsa_buf.len, 0);
        set_sqe_data_and_flag(sqe, io, 0);
    }

    void async_receive_from(IOData* io) {
         async_receive(io);
    }

    void async_send(IOData* io) {
        io->op = IOOP::Send;
        io_uring_sqe* sqe = ::io_uring_get_sqe(&ring_);
        ::io_uring_prep_send(sqe, io->fd, io->wsa_buf.buf, io->wsa_buf.len, 0);
        set_sqe_data_and_flag(sqe, io, 0);
    }

    void async_send_to(IOData* io) {
        async_send(io);
    }

    std::error_code poll(size_t timeout) {
        io_uring_cqe* cqe = nullptr;
        int ret = 0;

        ::io_uring_submit(&ring_);
        // int ret = ::io_uring_peek_cqe(&ring_, &cqe);
        //int ret = ::io_uring_submit_and_wait(&ring_, 1);
        if (0 == timeout) {
            ret = ::io_uring_peek_cqe(&ring_, &cqe);
        } else {
            ret = ::io_uring_wait_cqe(&ring_, &cqe);
        }
        
        if (-ETIME == ret || -EAGAIN == ret || -EINTR == ret) {
            return {};
        }

        if (ret != 0) {
            return MAGIO_SYSTEM_ERROR;
        }

        unsigned head;
        unsigned count = 0;

        io_uring_for_each_cqe(&ring_, head, cqe) {
            IOData* io = nullptr;
            std::memcpy(&io, &cqe->user_data, sizeof(cqe->user_data));

            std::error_code ec;
            if (IOOP::Accept == io->op) {
                int connfd = cqe->res;

                if (0 > connfd) {
                    ec = make_linux_system_error(-2);
                } else {
                    io->fd = connfd;
                    ec = set_nonblock(connfd);
                }

                io->cb(ec, io->ptr, connfd);
            } else if (IOOP::Connect == io->op) {
                ec = get_error(io->fd);
                io->cb(ec, io->ptr, io->fd);
            } else if (IOOP::Receive == io->op) {
                int bytes_read = cqe->res;
                ec = get_error(io->fd);
                if (bytes_read == 0) {
                    ec = make_linux_system_error(-1);
                }
                
                if (!ec) {
                    io->wsa_buf.len = bytes_read;
                }

                io->cb(ec, io->ptr, io->fd);
            } else if (IOOP::Send == io->op) {
                int bytes_write = cqe->res;
                ec = get_error(io->fd);
                if (!ec) {
                    io->wsa_buf.len = bytes_write;
                }
                
                io->cb(ec, io->ptr, io->fd);
            }
            ++count; 
        }

        ::io_uring_cq_advance(&ring_, count);
        return {};
    }

    std::atomic_size_t* count;
private:
    void set_sqe_data_and_flag(io_uring_sqe* sqe, IOData* io, unsigned flag) {
        ::io_uring_sqe_set_data(sqe, io);
        ::io_uring_sqe_set_flags(sqe, 0);
    }

    io_uring            ring_;
};

#endif

#ifdef _WIN32

class IOService {
public:
    IOService() = default;
    IOService(std::atomic_size_t& p): count(&p) { }

    std::error_code open() {
        plat::WebSocket::init();
        void* accept_ex_ptr = nullptr;
        void* connect_ex_ptr = nullptr;
        void* get_sock_addr_ptr = nullptr;

        if (auto ec = iocp_.open()) {
            return ec;
        }

        auto sock = make_socket(Protocol::TCP);
        if (MAGIO_INVALID_SOCKET == sock) {
            return MAGIO_SYSTEM_ERROR;
        }

        auto sock_guard = ScopeGuard{
            &sock,
            [](plat::socket_type* p) {
                close_socket(*p);
            }
        };

        GUID GuidAcceptEx = WSAID_ACCEPTEX;
        DWORD bytes_return;
        if (0 != ::WSAIoctl(
            sock, 
            SIO_GET_EXTENSION_FUNCTION_POINTER, 
            &GuidAcceptEx, 
            sizeof(GuidAcceptEx), 
            &accept_ex_ptr, 
            sizeof(accept_ex_ptr), 
            &bytes_return, 
            NULL, 
            NULL))
        {
            return MAGIO_SYSTEM_ERROR;
        }

        GUID GUidConnectEx = WSAID_CONNECTEX;
        if (0 != ::WSAIoctl(
            sock, 
            SIO_GET_EXTENSION_FUNCTION_POINTER, 
            &GUidConnectEx, 
            sizeof(GUidConnectEx), 
            &connect_ex_ptr, 
            sizeof(connect_ex_ptr), 
            &bytes_return, 
            NULL, 
            NULL))
        {
            return MAGIO_SYSTEM_ERROR;
        }

        GUID GuidGetAcceptExSockAddrs = WSAID_GETACCEPTEXSOCKADDRS;
        if (0 != ::WSAIoctl(
            sock, 
            SIO_GET_EXTENSION_FUNCTION_POINTER, 
            &GuidGetAcceptExSockAddrs, 
            sizeof(GuidGetAcceptExSockAddrs), 
            &get_sock_addr_ptr, 
            sizeof(get_sock_addr_ptr), 
            &bytes_return, 
            NULL, 
            NULL))
        {
            return MAGIO_SYSTEM_ERROR;
        }

        accept_ex_ = (LPFN_ACCEPTEX)accept_ex_ptr;
        connect_ex_ = (LPFN_CONNECTEX)connect_ex_ptr;
        get_acc_addr_ = (LPFN_GETACCEPTEXSOCKADDRS)get_sock_addr_ptr;
        
        return {};
    }

    void close() {
        iocp_.close();
    }

    std::error_code relate(SOCKET fd) {
        return iocp_.relate(fd);
    }

    void async_accept(SOCKET fd, IOData* io) {
        io->op = IOOP::Accept;
        ZeroMemory(&io->overlapped, sizeof(OVERLAPPED));

        bool status = accept_ex_(
            fd,
            io->fd,
            io->wsa_buf.buf,
            0,
            sizeof(sockaddr_in) + 16,
            sizeof(sockaddr_in) + 16,
            NULL,
            (LPOVERLAPPED)&io->overlapped
        );

        if (!status && ERROR_IO_PENDING != ::GetLastError()) {
            io->cb(MAGIO_SYSTEM_ERROR, io->ptr, fd);
            return;
        }
    }

    void async_connect(IOData* io, const sockaddr_in& addr) {
        io->op = IOOP::Connect;
        ZeroMemory(&io->overlapped, sizeof(OVERLAPPED));

        // 远端
        // sockaddr_in remote_addr{};
        // remote_addr.sin_family = AF_INET;
        // remote_addr.sin_port = ::htons(port);
        // if (-1 == ::inet_pton(AF_INET, host, &remote_addr.sin_addr)) {
        //     io->cb(MAGIO_SYSTEM_ERROR, io->ptr, io->fd);
        //     return;
        // }

        // data->fd target
        bool status = connect_ex_(
            io->fd,
            (sockaddr*)&addr,
            sizeof(sockaddr),
            NULL,
            0,
            NULL,
            (LPOVERLAPPED)&io->overlapped
        );

        if (!status && ERROR_IO_PENDING != ::GetLastError()) {
            io->cb(MAGIO_SYSTEM_ERROR, io->ptr, io->fd);
        }
    }

    void async_receive(IOData* io) {
        io->op = IOOP::Receive;
        ZeroMemory(&io->overlapped, sizeof(OVERLAPPED));

        DWORD flag = 0;
        int status = ::WSARecv(
            io->fd, 
            (LPWSABUF)&io->wsa_buf, 
            1, 
            NULL,
            &flag, 
            (LPOVERLAPPED)&io->overlapped, 
            NULL);
    
        if (SOCKET_ERROR == status && ERROR_IO_PENDING != ::GetLastError()) {
            io->cb(MAGIO_SYSTEM_ERROR, io->ptr, io->fd);
        }
    }

    void async_receive_from(IOData* io) {
        io->op = IOOP::Receive;
        ZeroMemory(&io->overlapped, sizeof(OVERLAPPED));

        DWORD flag = 0;
        int status = ::WSARecvFrom(
            io->fd, 
            &io->wsa_buf, 
            1,
            NULL, 
            &flag, 
            (sockaddr*)&io->remote, 
            &detail::sockaddr_in_len, 
            (LPOVERLAPPED)&io->overlapped, 
            NULL);
        
        if (SOCKET_ERROR == status && ERROR_IO_PENDING != ::GetLastError()) {
            io->cb(MAGIO_SYSTEM_ERROR, io->ptr, io->fd);
        }
    }

    void async_send(IOData* io) {
        io->op = IOOP::Send;
        ZeroMemory(&io->overlapped, sizeof(OVERLAPPED));

        DWORD flag = 0;
        int status = ::WSASend(
            io->fd, 
            &io->wsa_buf, 
            1, 
            NULL, 
            flag, 
            (LPOVERLAPPED)&io->overlapped, 
            NULL);
        
        if (SOCKET_ERROR == status && ERROR_IO_PENDING != ::GetLastError()) {
            io->cb(MAGIO_SYSTEM_ERROR, io->ptr, io->fd);
        }
    }

    void async_send_to(IOData* io) {
        io->op = IOOP::Send;
        ZeroMemory(&io->overlapped, sizeof(OVERLAPPED));

        DWORD flag = 0;
        int status = ::WSASendTo(
            io->fd,
            &io->wsa_buf, 
            1, 
            NULL, 
            flag, 
            (const sockaddr *)&io->remote, 
            sizeof(io->remote), 
            (LPOVERLAPPED)&io->overlapped,
            NULL);
        
        if (SOCKET_ERROR == status && ERROR_IO_PENDING != ::GetLastError()) {
            io->cb(MAGIO_SYSTEM_ERROR, io->ptr, io->fd);
        }
    }

    std::error_code poll(size_t timeout) {
        DWORD bytes_transferred = 0;
        IOData* iodata = nullptr;

        auto ec = iocp_.wait(timeout, &bytes_transferred, (void**)&iodata);
        if (ec.value() == WAIT_TIMEOUT) {
            return {};
        }

        if (!iodata) {
            return ec;
        }

        switch (iodata->op) {
        case IOOP::Accept: {
            DEBUG_LOG("do accept");

            LPSOCKADDR local_addr_ptr;
            LPSOCKADDR remote_addr_ptr;
            int local_addr_len = 0;
            int remote_addr_len = 0;

            get_acc_addr_(
                iodata->wsa_buf.buf,
                0,
                sizeof(sockaddr_in) + 16,
                sizeof(sockaddr_in) + 16,
                &local_addr_ptr,
                &local_addr_len,
                &remote_addr_ptr,
                &remote_addr_len
            );

            iodata->local = *((sockaddr_in*)local_addr_ptr);
            iodata->remote = *((sockaddr_in*)remote_addr_ptr);

            iodata->cb(ec, iodata->ptr, iodata->fd);
            break;
        } 
        case IOOP::Connect: {
            DEBUG_LOG("do connect");
            iodata->cb(ec, iodata->ptr, iodata->fd);
            break;
        }
        case IOOP::Receive:
            DEBUG_LOG("do receive");
            iodata->wsa_buf.len = bytes_transferred;
            if (bytes_transferred == 0) {
                ec = make_win_system_error(-1);
            }
            iodata->cb(ec, iodata->ptr, iodata->fd);
            break;
        case IOOP::Send:
            DEBUG_LOG("do send");
            iodata->wsa_buf.len = bytes_transferred;
            iodata->cb(ec, iodata->ptr, iodata->fd);
            break;
        default:
            break;
        }

        return {};
    }

    std::error_code poll2(size_t timeout) {
        return poll(timeout);
    }
    
    std::atomic_size_t*             count;
private:
    IOCompletionPort                iocp_;

    LPFN_ACCEPTEX                   accept_ex_;
    LPFN_CONNECTEX                  connect_ex_;
    LPFN_GETACCEPTEXSOCKADDRS       get_acc_addr_;
};

using SubIOService = IOService;

#endif

}

}

