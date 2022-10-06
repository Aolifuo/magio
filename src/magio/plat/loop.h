#pragma once

#ifdef __linux__

#include <netinet/in.h>
#include "magio/dev/Log.h"
#include "magio/core/Queue.h"
#include "magio/plat/io.h"
#include "magio/plat/errors.h"
#include "magio/plat/epoll/epoll.h"

namespace magio {

namespace plat {

struct EventData {
    int     fd;
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
        if (!conn_fds_.empty()) {
            int fd = conn_fds_.front();
            conn_fds_.pop();
            data->cb({}, data->data, fd);
        } else {
            acc_.emplace(data);
        }
    }

    void async_connect(int fd, EventData* data) {
        DEBUG_LOG("async_connect");
        auto ec = update_epoll(EPOLL_CTL_ADD, EPOLLIN | EPOLLOUT | EPOLLET, fd, data);
        if (ec) {
            data->cb(ec, data->data, fd);
        }
    }

    void async_receive(int fd, EventData* data) {
        DEBUG_LOG("async_receive");
        auto ec = update_epoll(EPOLL_CTL_MOD, EPOLLIN, fd, data);
        if (ec) {
            data->cb(ec, data->data, fd);
        }
    }

    void async_send(int fd, EventData* data) {
        DEBUG_LOG("async_send");
        auto ec = update_epoll(EPOLL_CTL_MOD, EPOLLOUT, fd, data);
        if (ec) {
            data->cb(ec, data->data, fd);
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
            if (events_[i].events & EPOLLERR) {
                handle_error(events_[i]);
                continue;
            }

            // accept
            if (events_[i].data.fd == fd_) {
                do_accept();
                continue;
            }

            // connect
            if (-2 == fd_) {
                do_connect(events_[i]);
            }

            // send or recv
            if (events_[i].events & EPOLLIN) {
                do_receive(events_[i]);
            } 

            if (events_[i].events & EPOLLOUT) {
                do_send(events_[i]);
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

    void do_connect(::epoll_event& ev) {
        DEBUG_LOG("do connect");
        auto data = (EventData*)ev.data.ptr;
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

    void do_receive(::epoll_event& ev) {
        auto data = (EventData*)ev.data.ptr;

        std::error_code ec;
        auto rdlen = ::recv(data->fd, data->io->input_buffer.buf, global_config.buffer_size, 0);
        if (-1 == rdlen) {
            ec = SYSTEM_ERROR;
        } else if (0 == rdlen) {
            ec = std::make_error_code(std::errc::operation_canceled);
        } else {
            data->io->input_buffer.len = rdlen;
        }
        
        data->cb(ec, data->data, ev.data.fd);
    }

    void do_send(::epoll_event& ev) {
        auto data = (EventData*)ev.data.ptr;

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

        data->cb(ec, data->data, ev.data.fd);
    }

    void handle_error(::epoll_event& ev) {
        DEBUG_LOG("epoll event err");
        if (!ev.data.ptr) {
            return;
        }
        auto data = (EventData*)ev.data.ptr;

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

}

}

#endif