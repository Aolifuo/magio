#pragma once

#include "magio/Configs.h"
#include "magio/core/ObjectPool.h"
#include "magio/net/Buffer.h"

namespace magio {

namespace plat {

struct IOData {
    int fd;
    Buffer input_buffer;
    Buffer output_buffer;
};

auto& global_io() {
    static SyncObjectPool<IOData> pool(
        global_config.max_sockets / 2 + 1, 
        global_config.max_sockets
    );

    return pool;
}

}

}
