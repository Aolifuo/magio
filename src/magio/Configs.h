#pragma once

#include <cstddef>

namespace magio {

struct GlobalConfig {
    size_t max_sockets = 1024;
    size_t worker_threads = 16;
    size_t default_buffers = 512;

    static constexpr size_t buffer_size = 1024 * 4;
};

inline GlobalConfig global_config;

}

