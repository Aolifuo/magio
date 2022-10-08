#pragma once

#ifdef __linux__
#include <netinet/in.h>
#include "magio/plat/epoll/epoll.h"
#endif

#ifdef _WIN32
#include "magio/plat/iocp/iocp.h"
#include <Ws2tcpip.h>
#include <MSWSock.h>
#endif

#include "magio/dev/Log.h"
#include "magio/core/Queue.h"
#include "magio/plat/io.h"
#include "magio/plat/declare.h"
#include "magio/plat/errors.h"


namespace magio {

namespace plat {
#ifdef __linux__

struct EventData {
    int     fd;
    IOOP    op;
    IOData* io;
    void*   data;
    void(*cb)(std::error_code, void*, int);
};

// A Wrapper
class IOLoop {

    IOLoop(int fd, size_t ev_size, Epoll&& ep)
    : fd_(fd)
    , epoll_(std::move(ep))
    , events_(ev_size)
    {

    }
public:
    // fd = -1 => no thing,  fd = -2 => connect
    static Expected<IOLoop> create(size_t max_event, int fd = -1) {
        auto expect_epoll = Epoll::create((int)max_event);
        if (!expect_epoll) {
            return expect_epoll.unwrap_err();
        }
        auto epoll = expect_epoll.unwrap();

        // listener
        if (fd > 0) {
            ::epoll_event ev{};
            ev.events = EPOLLIN;
            ev.data.fd = fd;

            if (auto ec = epoll.add(fd, ev)) {
                return ec;
            }
        }
        
        return IOLoop{fd, max_event, std::move(epoll)};
    }

    void close() {
        epoll_.close();
    }
    
    std::error_code add(int fd) {
        return update_epoll(EPOLL_CTL_ADD, 0, fd, nullptr);
    }

    std::error_code remove(int fd) {
        return update_epoll(EPOLL_CTL_DEL, 0, fd, nullptr);
    }

    void async_accept(EventData* data) {
        DEBUG_LOG("async_accept");
        data->op = IOOP::Accept;
        if (!conn_fds_.empty()) {
            int fd = conn_fds_.front();
            conn_fds_.pop();
            data->cb({}, data->data, fd);
        } else {
            acc_.emplace(data);
        }
    }

    void async_connect(EventData* data) {
        DEBUG_LOG("async_connect");
        data->op = IOOP::Connect;
        auto ec = update_epoll(EPOLL_CTL_ADD, EPOLLOUT, data->fd, data);
        if (ec) {
            data->cb(ec, data->data, fd);
        }
    }

    void async_receive(EventData* data) {
        DEBUG_LOG("async_receive");
        data->op = IOOP::Receive;
        auto ec = update_epoll(EPOLL_CTL_MOD, EPOLLIN, data->fd, data);
        if (ec) {
            data->cb(ec, data->data, data->fd);
        }
    }

    void async_send(EventData* data) {
        DEBUG_LOG("async_send");
        data->op = IOOP::Send;
        auto ec = update_epoll(EPOLL_CTL_MOD, EPOLLOUT, data->fd, data);
        if (ec) {
            data->cb(ec, data->data, data->fd);
        }
    }

    std::error_code poll(int timeout) {
        auto expect_len = epoll_.wait(events_.data(), (int)events_.size(), timeout);
        if (!expect_len) {
            return expect_len.unwrap_err();
        }
        auto evlen = expect_len.unwrap();

        if (evlen == 0) {
            return {};
        }

        DEBUG_LOG("have events: ");

        for (int i = 0; i < evlen; ++i) {
            // accept
            if (events_[i].data.fd == fd_) {
                do_accept();
                continue;
            }

            auto data = (EventData*)events_[i].data.ptr;

            // connect
            if (data->op == IOOP::Connect) {
                do_connect(data);
                continue;
            }

            // send or recv
            if (events_[i].events & EPOLLIN) {
                do_receive(data);
            } else if (events_[i].events & EPOLLOUT) {
                do_send(data);
            } else if (events_[i].events & EPOLLERR) {
                handle_error(data);
            }
        }

        return {};
    }

private:
    void do_accept() {
        DEBUG_LOG("do accept");
        ::sockaddr_in addr{};
        ::socklen_t len = sizeof(sockaddr_in);
        int connfd = ::accept(fd_, (::sockaddr*)&addr, &len);
        if (-1 == connfd) {
            std::printf("accept error: %s\n", strerror(errno));
            return;
        }

        if (!acc_.empty()) {
            auto data = acc_.front();
            acc_.pop();
            data->cb({}, data->data, connfd);
        } else {
            conn_fds_.push(connfd);
        }
    }

    void do_connect(EventData* data) {
        DEBUG_LOG("do connect");

        std::error_code ec;
        int error = 0;
        socklen_t len = sizeof(error);
        if (-1 == getsockopt(data->fd, SOL_SOCKET, SO_ERROR, &error, &len)) {
            ec = SYSTEM_ERROR;
        } else if (0 != error) {
            ec = std::make_error_code((std::errc)error);
        }

        data->cb(ec, data->data, data->fd);
    }

    void do_receive(EventData* data) {

        std::error_code ec;
        auto rdlen = ::recv(data->fd, data->io->input_buffer.buf, global_config.buffer_size, 0);
        if (-1 == rdlen) {
            ec = SYSTEM_ERROR;
        } else if (0 == rdlen) {
            ec = std::make_error_code(std::errc::operation_canceled);
        } else {
            data->io->input_buffer.len = rdlen;
        }
        
        data->cb(ec, data->data, data->fd);
    }

    void do_send(EventData* data) {

        std::error_code ec;
        auto wlen = ::send(data->fd, data->io->output_buffer.buf, data->io->output_buffer.len, 0);
        
        if (-1 == wlen) {
            ec = SYSTEM_ERROR;
        } else if (0 == wlen) {
            ec = std::make_error_code(std::errc::operation_canceled);
        } else {
            data->io->output_buffer.len = wlen;
        }
        
        ec = update_epoll(EPOLL_CTL_MOD, 0, data->fd, nullptr);

        data->cb(ec, data->data, data->fd);
    }

    void handle_error(EventData* data) {
        DEBUG_LOG("epoll event err");
        if (!data) {
            return;
        }

        std::error_code ec;
        int error = 0;
        socklen_t errlen = sizeof(error);
        if (-1 == getsockopt(data->fd, SOL_SOCKET, SO_ERROR, &error, &errlen)) {
            ec = SYSTEM_ERROR;
        } else {
            ec = std::make_error_code((std::errc)error);
        }

        data->cb(ec, data->data, data->fd);
    }

    std::error_code update_epoll(int op, uint32_t events, int fd, void* hook) {
        ::epoll_event ev{};
        ev.data.ptr = hook;
        ev.events = events;

        std::error_code ec;
        switch (op) {
        case EPOLL_CTL_ADD:
            ec = epoll_.add(fd, ev);
            break;
        case EPOLL_CTL_MOD:
            ec = epoll_.update(fd, ev);
            break;
        case EPOLL_CTL_DEL:
            ec = epoll_.remove(fd);
            break;
        default:
            break;
        }

        return ec;
    }

    int                         fd_;
    Epoll                       epoll_;

    std::vector<::epoll_event>  events_;

    RingQueue<EventData*>       acc_;
    RingQueue<int>              conn_fds_;
};

#endif

#ifdef _WIN32

class IOLoop {
    IOLoop(IOCompletionPort&& iocp, void* accpet_ex, void* connect_ex)
        : iocp_(std::move(iocp))
        , accept_ex_((LPFN_ACCEPTEX)accpet_ex)
        , connect_ex_((LPFN_CONNECTEX)connect_ex)
    { }
public:
    IOLoop(IOLoop&& other) noexcept = default;
    IOLoop& operator=(IOLoop&& other) noexcept = default;

    static Expected<IOLoop> create(SOCKET sock) {
        GUID GuidAcceptEx = WSAID_ACCEPTEX;
        DWORD bytes_return;
        void* accept_ex_ptr = nullptr;
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
            return SYSTEM_ERROR;
        }

        GUID GUidConnectEx = WSAID_CONNECTEX;
        void* connect_ex_ptr = nullptr;
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
            return SYSTEM_ERROR;
        }

        auto iocp = IOCompletionPort::create();
        if (!iocp) {
            return iocp.unwrap_err();
        }
        return IOLoop{iocp.unwrap(), accept_ex_ptr, connect_ex_ptr};
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
            io->input_buffer.buf,
            0,
            sizeof(sockaddr_in) + 16,
            sizeof(sockaddr_in) + 16,
            NULL,
            (LPOVERLAPPED)&io->overlapped
        );

        if (!status && ERROR_IO_PENDING != ::GetLastError()) {
            io->cb(SYSTEM_ERROR, io->ptr, fd);
            return;
        }
    }

    void async_connect(IOData* io, const char* host, uint_least16_t port) {
        io->op = IOOP::Connect;
        ZeroMemory(&io->overlapped, sizeof(OVERLAPPED));

        // 远端
        sockaddr_in remote_addr{};
        remote_addr.sin_family = AF_INET;
        remote_addr.sin_port = ::htons(port);
        if (-1 == ::inet_pton(AF_INET, host, &remote_addr.sin_addr)) {
            io->cb(SYSTEM_ERROR, io->ptr, io->fd);
            return;
        }

        // data->fd target
        bool status = connect_ex_(
            io->fd,
            (sockaddr*)&remote_addr,
            sizeof(sockaddr),
            NULL,
            0,
            NULL,
            (LPOVERLAPPED)&io->overlapped
        );

        if (!status && ERROR_IO_PENDING != ::GetLastError()) {
            io->cb(SYSTEM_ERROR, io->ptr, io->fd);
        }

    }

    void async_receive(IOData* io) {
        io->op = IOOP::Receive;
        ZeroMemory(&io->overlapped, sizeof(OVERLAPPED));
        io->wsa_buf.buf = io->input_buffer.buf;
        io->wsa_buf.len = global_config.buffer_size;

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
            io->cb(SYSTEM_ERROR, io->ptr, io->fd);
        }
    }

    void async_send(IOData* io) {
        io->op = IOOP::Send;
        ZeroMemory(&io->overlapped, sizeof(OVERLAPPED));
        io->wsa_buf.buf = io->output_buffer.buf;
        io->wsa_buf.len = (ULONG)io->output_buffer.len;

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
            io->cb(SYSTEM_ERROR, io->ptr, io->fd);
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
            iodata->input_buffer.len = bytes_transferred;
            if (bytes_transferred == 0) {
                ec = make_win_system_error(-1);
            }
            iodata->cb(ec, iodata->ptr, iodata->fd);
            break;
        case IOOP::Send:
            DEBUG_LOG("do send");
            iodata->output_buffer.len = bytes_transferred;
            if (bytes_transferred == 0) {
                ec = make_win_system_error(-1);
            }
            iodata->cb(ec, iodata->ptr, iodata->fd);
            break;
        default:
            break;
        }

        return {};
    }

    void close() {
        iocp_.close();
    }
private:
    IOCompletionPort                            iocp_;

    LPFN_ACCEPTEX                               accept_ex_;
    LPFN_CONNECTEX                              connect_ex_;
};

#endif

}

}

