#pragma once

#include "magio/EventLoop.h"
#include <cstdio>

namespace magio {

AnyExecutor default_executor() {
    static EventLoop loop;
    std::printf("aaaa\n");
    return loop.get_executor();
}

}