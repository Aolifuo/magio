# Magio

Magio是一个基于C++20实现的协程网络库，包含异步文件IO，网络IO（Tcp、Udp）等。
支持Windows和Linux（magioV3）平台。

## Magio V3

### Tcp server

```cpp
Coro<> handle_conn(net::Socket sock) {
    char buf[1024];
    for (; ;) {
        error_code ec;
        size_t rd = co_await sock.receive(buf, sizeof(buf)) | throw_err;
        if (rd == 0) {
            M_INFO("{}", "EOF");
            break;
        }
        M_INFO("receive: {}", string_view(buf, rd));
        co_await sock.send(buf, rd) | throw_err;
    }
}

Coro<> server() {
    net::EndPoint local(net::make_address("::1") | panic_on_err, 1234);
    auto acceptor = net::Acceptor::listen(local) | panic_on_err;

    for (; ;) {
        error_code ec;
        auto [socket, peer] = co_await acceptor.accept() | get_code(ec);
        if (ec) {
            M_ERROR("{}", ec.message());
        } else {
            M_INFO("accept [{}]:{}", peer.address().to_string(), peer.port());
            this_context::spawn(handle_conn(std::move(socket)), [](exception_ptr eptr, Unit) {
                try {
                    try_rethrow(eptr);
                } catch(const std::system_error& err) {
                    M_ERROR("{}", err.what());
                }
            });
        }
    }
}

int main() {
    CoroContext ctx(128);
    this_context::spawn(server());
    ctx.start();
}
```

### Tcp Client

```cpp
Coro<> client() {
    net::EndPoint local(net::make_address("::1") | throw_err, 0);
    net::EndPoint peer(net::make_address("::1") | throw_err, 1234);
    auto socket = net::Socket::open(net::Ip::v6, net::Transport::Tcp) | throw_err;

    socket.bind(local) | throw_err;
    co_await socket.connect(peer) | throw_err;
 
    char buf[1024];
    for (int i = 0; i < 5; ++i) {
        co_await socket.send("hello server", 12) | throw_err;
        size_t rd = co_await socket.receive(buf, sizeof(buf)) | throw_err;
        if (rd == 0) {
            M_ERROR("{}", "EOF");
            break;
        }
        M_INFO("{}", string_view(buf, rd));
    }
}

int main() {
    CoroContext ctx(128);
    this_context::spawn(client(), [](exception_ptr eptr, Unit) {
        try {
            try_rethrow(eptr);
        } catch(const system_error& err) {
            M_ERROR("{}", err.what());
        }
        this_context::stop();
    });
    ctx.start();
}
```

### Udp echo

```cpp
Coro<> amain() {
    net::EndPoint local(net::make_address("::1") | throw_err, 1234);
    auto socket = net::Socket::open(net::Ip::v6, net::Transport::Udp) | throw_err;
    socket.bind(local) | panic_on_err;

    char buf[1024];
    for (; ;) {
        auto [rd, peer] = co_await socket.receive_from(buf, sizeof(buf)) | throw_err;
        M_INFO("[{}]:{}: {}", peer.address().to_string(), peer.port(), string_view(buf, rd));
        co_await socket.send_to(buf, rd, peer) | throw_err;
    }
}

int main() {
    CoroContext ctx(128);
    this_context::spawn(amain(), [](exception_ptr eptr, Unit) {
        try {
            try_rethrow(eptr);
        } catch(const std::system_error& err) {
            M_ERROR("{}", err.what());
        }
        this_context::stop();
    });
    ctx.start();    
}
```

### Copy file

```cpp
Coro<> copyfile() {
    auto from = File::open("from", File::ReadOnly);
    auto to = File::open("to", File::WriteOnly | File::Create | File::Truncate);
    if (!from || !to) {
        M_FATAL("cannot open file {} or {}", "from", "to");
    }

    char buf[1024];
    for (; ;) {
        error_code ec;
        size_t rd = co_await from.read(buf, sizeof(buf)) | throw_err;
        if (rd == 0) {
            break;
        }
        co_await to.write(buf, rd) | throw_err;
    }
}

int main() {
    CoroContext ctx(128);
    this_context::spawn(copyfile(), [](exception_ptr eptr, Unit) {
        try {
            try_rethrow(eptr);
        } catch(const system_error& err) {
            M_ERROR("{}", err.what());
        }
        this_context::stop();
    });
    ctx.start();
}
```

### Fork join sort

```cpp
template<typename Iter, typename Pred>
Coro<> fork_join_sort(ThreadPool& exe, Iter bg, Iter ed, Pred pred, size_t times) {
    if (bg >= ed) {
        co_return;
    }

    if (times > 0) {
        auto mid = bg + (ed - bg) / 2;
        nth_element(bg, mid, ed, pred);
        co_await join(
            fork_join_sort(exe, bg, mid, pred, times - 1),
            fork_join_sort(exe, mid + 1, ed, pred, times - 1)
        );
    } else {
        co_await exe.spawn_blocking([bg, ed, pred] {
            sort(bg, ed, pred);
        });
    }
    co_return;
}

Coro<> amain(ThreadPool& pool) {
    vector<int> vec(1e8);
    random_device dev;
    default_random_engine eng(dev());
    uniform_int_distribution<> uid;
    for (auto& v : vec) {
        v = uid(eng);
    }
    
    auto bg = TimerClock::now();
    co_await fork_join_sort(pool, vec.begin(), vec.end(), less<>(), 3);
    auto dif = TimerClock::now() - bg;
    M_INFO("{}", dif);

    co_return this_context::stop();
}

int main() {
    CoroContext ctx(128);
    ThreadPool pool(8);
    ctx.spawn(amain(pool));
    pool.start();
    ctx.start();
}
```

## Benchmark

以下为V2的性能

单线程模式下，使用Apache Benchmarking工具。设置1000并发量，1000000请求数，keepalive，RPS如下。

| framework      |  RPS [#/sec] (mean) | Language |   Pattern |
|----------------|--------------------:| --------: |----------:|
| [C++ asio](docs/benchmark.md#cpp-asio)        | 42389.54  | C++           | coroutine |
| [C++ libuv](docs/benchmark.md#cpp-libuv)      | 43397.97  | C++           | coroutine |
| [C++ magio](docs/benchmark.md#cpp-magio)      | 43068.93  | C++           | coroutine |
| [Rust tokio](docs/benchmark.md#rust-tokio)    | 46215.20  | Rust          | coroutine |
| [NodeJs](docs/benchmark.md#nodejs)            | 39292.92  | JavaScript    | eventloop |

More detail see: [benchmark.md](docs/benchmark.md)

在16个线程模式下，使用wrk工具。设置8线程，100000连接数，持续10s, RPS如下。

| framework      |  RPS [#/sec] (mean) | Language |   Pattern |
|----------------|--------------------:| --------: |----------:|
| [C++ asio](docs/benchmark2.md#cpp-asio)        | 164650.34  | C++           | coroutine |
| [C++ magio](docs/benchmark2.md#cpp-magio)      | 165560.01  | C++           | coroutine |
| [Golang](docs/benchmark2.md#golang)            | 161929.66  | Golang        | coroutine |

More detail see: [benchmark2.md](docs/benchmark2.md)

V3 TODO
