#pragma once

#include "magio/Configs.h"
#include "magio/dev/Resource.h"
#include "magio/core/ObjectPool.h"
#include "magio/net/Buffer.h"

#ifdef _WIN32
#include "magio/plat/declare.h"
#include <WinSock2.h>
#endif

namespace magio {

namespace plat {

#ifdef __linux__
struct IOData {
    int fd;
    Buffer input_buffer;
    Buffer output_buffer;
};
#endif

#ifdef _WIN32
struct IOData {
    OVERLAPPED overlapped;
    SOCKET fd;
    IOOP op;
    void* ptr;
    void(*cb)(std::error_code, void*, SOCKET);

    WSABUF wsa_buf;
    Buffer input_buffer;
    Buffer output_buffer;
};
#endif


class GlobalIO {
    static auto& global_io() {
        static SyncObjectPool<IOData> pool(
            global_config.default_buffers, 
            global_config.default_buffers
        );

        return pool;
    }
public:
    static auto get() {
        MAGIO_GET_BUFFER;
        return global_io().get();
    }

    static auto put(IOData* data) {
        MAGIO_PUT_BUFFER;
        global_io().put(data);
    } 
};

}

}
