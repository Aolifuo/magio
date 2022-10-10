#pragma once

#include <system_error>

namespace magio {

namespace plat {

class IOLoop;

class Runtime;

}

// Wrapper
class IOService {
public:
    IOService() = default;

    IOService(plat::IOLoop* loop, plat::Runtime* runtime, std::once_flag* f)
        : loop_(loop)
        , runtime_(runtime)
        , once_f_(f)
    {  }

    std::error_code open();

    plat::IOLoop* get_loop() {
        return loop_;
    }
private:
    plat::IOLoop* loop_;
    plat::Runtime* runtime_;
    std::once_flag* once_f_;
};

}