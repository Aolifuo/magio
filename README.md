# Magio

Magio是一个基于C++20实现的协程网络库，包含异步文件IO，网络IO（Tcp、Udp）等。
支持Windows和Linux（magioV3）平台。

## Magio V3

### Tcp server

```cpp
Coro<> handle_connection(net::Socket sock) {
    char buf[1024];
    for (; ;) {
        error_code ec;
        size_t rd = co_await sock.receive(buf, sizeof(buf), ec);
        if (ec || rd == 0) {
            M_ERROR("recv error: {}", ec ? ec.message() : "EOF");
            break;
        }
        M_INFO("receive: {}", string_view(buf, rd));
        co_await sock.send(buf, rd, ec);
        if (ec) {
            M_ERROR("send error: {}", ec.message());
            break;
        }
    }
}

Coro<> server() {
    error_code ec;
    net::EndPoint local(net::make_address("::1", ec), 1234);
    if (ec) {
        M_FATAL("{}", ec.message());
    }

    net::Acceptor acceptor;
    acceptor.bind_and_listen(local, ec);
    if (ec) {
        M_FATAL("{}", ec.message());
    }

    for (; ; ec.clear()) {
        auto [socket, peer] = co_await acceptor.accept(ec);
        if (ec) {
            M_ERROR("{}", ec.message());
        } else {
            M_INFO("accept [{}]:{}", peer.address().to_string(), peer.port());
            this_context::spawn(handle_connection(std::move(socket)));
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
    error_code ec;
    net::EndPoint local(net::make_address("::1", ec), 0);
    net::EndPoint peer(net::make_address("::1", ec), 1234);
    if (ec) {
        M_FATAL("{}", ec.message());
    }

    net::Socket socket;
    socket.open(net::Ip::v6, net::Transport::Tcp, ec);
    socket.bind(local, ec);
    if (ec) {
        M_FATAL("{}", ec.message());
    }

    co_await socket.connect(peer, ec);
    if (ec) {
        M_FATAL("connect error: {}", ec.message());
    }
 
    char buf[1024];
    for (int i = 0; i < 5; ++i) {
        co_await socket.send("hello server", 12, ec);
        if (ec) {
            M_ERROR("send error: {}", ec.message());
            break;
        }
        size_t rd = co_await socket.receive(buf, sizeof(buf), ec);
        if (ec || rd == 0) {
            M_ERROR("recv error: {}", ec ? ec.message() : "EOF");
            break;
        }
        M_INFO("{}", string_view(buf, rd));
    }
    this_context::stop();
}

int main() {
    CoroContext ctx(128);
    this_context::spawn(client());
    ctx.start();
}
```

### Udp echo

```cpp
Coro<> amain() {
    error_code ec;
    net::Socket socket;
    socket.open(net::Ip::v6, net::Transport::Udp, ec);
    if (ec) {
        M_FATAL("{}", ec.message());
    }

    net::EndPoint local(net::make_address("::1", ec), 1234);
    socket.bind(local, ec);
    if (ec) {
        M_FATAL("{}", ec.message());
    }

    char buf[1024]{};
    for (; ; ec.clear()) {
        auto [rd, peer] = co_await socket.receive_from(buf, sizeof(buf), ec);
        if (ec) {
            M_ERROR("recv error: {}", ec.message());
            continue;
        }
        M_INFO("[{}]:{}: {}", peer.address().to_string(), peer.port(), string_view(buf, rd));
        co_await socket.send_to(buf, rd, peer, ec);
        if (ec) {
            M_ERROR("send error: {}", ec.message());
            continue;
        }
    }
}

int main() {
    CoroContext ctx(128);
    this_context::spawn(amain());
    ctx.start();    
}
```

### Copy file

```cpp
Coro<> copyfile() {
    File from("from", File::ReadOnly);
    File to("to", File::WriteOnly | File::Create | File::Truncate);
    if (!from || !to) {
        M_FATAL("cannot open file {} or {}", "from", "to");
    }

    char buf[1024];
    for (; ;) {
        error_code ec;
        size_t rd = co_await from.read(buf, sizeof(buf), ec);
        if (ec || rd == 0) {
            break;
        }
        co_await to.write(buf, rd, ec);
    }
    this_context::stop();
}

int main() {
    CoroContext ctx(128);
    this_context::spawn(copyfile());
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
