#pragma once

#ifdef __linux__

#include <cstddef>
#include <unistd.h>
#include <sys/epoll.h>

#include "magio/plat/errors.h"

namespace magio {

class Epoll {
public:
    Epoll() = default;
    
    Epoll(const Epoll&) = delete;

    ~Epoll() {
        close();
    }

    std::error_code open() {
        if (-1 != epfd_) {
            return {};
        }

        int fd = ::epoll_create(1);
        if (-1 == fd) {
            return MAGIO_SYSTEM_ERROR;
        }

        epfd_ = fd;
        return {};
    }

    void close() {
        if (-1 != epfd_) {
            ::close(epfd_);
            epfd_ = -1;
        }
    }

    std::error_code add(int fd, epoll_event& ev) {
        if (-1 == ::epoll_ctl(epfd_, EPOLL_CTL_ADD, fd, &ev)) {
            return MAGIO_SYSTEM_ERROR;
        }
        return {};
    }

    std::error_code update(int fd, epoll_event& ev) {
        if (-1 == ::epoll_ctl(epfd_, EPOLL_CTL_MOD, fd, &ev)) {
            return MAGIO_SYSTEM_ERROR;
        }
        return {};
    }

    std::error_code remove(int fd) {
        if (-1 == ::epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, nullptr)) {
            return MAGIO_SYSTEM_ERROR;
        }
        return {};
    }

    int wait(epoll_event* events, int max, int timeout) {
        return ::epoll_wait(epfd_, events, max, timeout);
    }
private:
    int epfd_ = -1;
};

}

#endif