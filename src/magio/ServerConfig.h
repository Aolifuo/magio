#pragma once

struct ServerConfig {
    size_t listener_num = 10;
    size_t max_socket_num = 100;
    size_t thread_num = 16;

    static constexpr size_t buffer_size = 1024 * 4;
};

inline ServerConfig server_config;
