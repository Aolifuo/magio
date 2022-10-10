#pragma once

#ifdef __linux__

#include <cstddef>
#include <unistd.h>
#include <sys/epoll.h>

#include "magio/dev/Log.h"
#include "magio/core/Error.h"

namespace magio {

class Epoll {

    Epoll(int epfd) {
        epfd_ = epfd;
    }

public:
    ~Epoll() {
        if (epfd_ != -1) {
            ::close(epfd_);
        }
    }

    Epoll(const Epoll&) = delete;

    Epoll(Epoll&& other) noexcept : epfd_(other.epfd_) {
        other.epfd_ = -1;
    }

    Epoll& operator=(Epoll&& other) noexcept {
        epfd_ = other.epfd_;
        other.epfd_ = -1;
        return *this;
    }

    static Expected<Epoll> create(int num) {
        int fd = ::epoll_create(num);
        if (-1 == fd) {
            return std::make_error_code((std::errc)errno);
        }
        return Epoll(fd);
    }

    std::error_code add(int fd, epoll_event& ev) {
        if (-1 == ::epoll_ctl(epfd_, EPOLL_CTL_ADD, fd, &ev)) {
            return std::make_error_code((std::errc)errno);
        }
        return {};
    }

    std::error_code update(int fd, epoll_event& ev) {
        if (-1 == ::epoll_ctl(epfd_, EPOLL_CTL_MOD, fd, &ev)) {
            return std::make_error_code((std::errc)errno);
        }
        return {};
    }

    std::error_code remove(int fd) {
        if (-1 == ::epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, nullptr)) {
            return std::make_error_code((std::errc)errno);
        }
        return {};
    }

    Expected<size_t> wait(epoll_event* events, int max, int timeout) {
        auto evlen = ::epoll_wait(epfd_, events, max, timeout);
        if (-1 == evlen) {
            return std::make_error_code((std::errc)errno);
        }
        return evlen;
    }

    void close() {
        if (-1 != epfd_) {
            ::close(epfd_);
            epfd_ = -1;
        }
    }
private:
    int epfd_;
};

}

#endif