#pragma once

#include "magio/Configs.h"

namespace magio {

struct Buffer {
    char buf[global_config.buffer_size];
    size_t len;
};

}