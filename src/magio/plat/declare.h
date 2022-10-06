#pragma once

#include <stdint.h>
#include <string>
#include "magio/Configs.h"

namespace magio {

namespace plat {

enum class TransportProtocol { TCP, UDP };

enum class IOOP {
    Noop,
    Accept,
    Receive,
    Send,
    Connect,
};

using socket_type = uint64_t;

}


}