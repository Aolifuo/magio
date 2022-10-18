# magio

magio是一个跨平台(Windows、Linux)的协程网络库，接口简单易用

## Udp echo

```cpp
Coro<> amain() {
    try {
        array<char, 1024> buf;
        auto socket = co_await UdpSocket::bind("127.0.0.1", 8001);
        for (; ;) {
            // 5秒后超时取消read
            auto read_res = co_await timeout(5000, socket.read_from(buf.data(), buf.size()));
            if (!read_res) {
                cout << "read timeout!\n";
                continue;
            }

            auto [len, address] = read_res.unwrap();
            cout << string_view(buf.data(), len) << '\n';

            auto write_res = co_await timeout(5000, socket.write_to(buf.data(), len, address));
            if (!write_res) {
                cout << "write timeout!\n";
                continue;
            }
        }
    } catch (const system_error& err) {
        cout << err.what() << '\n';
    }
}

int main() {
    Magico loop(1);
    co_spawn(loop.get_executor(), amain());
    loop.run();
}
```

## Tcp echo

### Client

```cpp
Coro<> amain(Magico& loop) {
    try {
        auto stream = co_await TcpStream::connect("127.0.0.1", 8000);

        cout << stream.local_address().to_string()
             << " connect "
             << stream.remote_address().to_string()
             << '\n';

        array<char, 1024> buf;
        for (int i = 0; i < 5; ++i) {
            auto [_, rdlen] = co_await (
                stream.write("Hello server..", 14) &&
                stream.read(buf.data(), buf.size())
            );
            cout << string_view(buf.data(), rdlen) << '\n';
        }
    } catch(const std::exception& err) {
        cout <<  err.what() << '\n';
    }

}

int main() {
    Magico loop(1);
    co_spawn(loop.get_executor(), amain(loop));
    loop.run();
}
```

output

```shell
127.0.0.1:49699 connect 127.0.0.1:8000
Hello client..
Hello client..
Hello client..
Hello client..
Hello client..
```

### Server

```cpp
Coro<> process(TcpStream stream) {
    try {
        array<char, 1024> buf;
        for (; ;) {
            auto [rdlen, _] = co_await (
                stream.read(buf.data(), buf.size()) &&
                stream.write("Hello client..", 14)
            );
            cout << string_view(buf.data(), rdlen) << '\n';
        }
    } catch(const std::runtime_error& err) {
        cout << err.what() << '\n';
    }
}

Coro<> amain() {
    try {
        auto server = co_await TcpServer::bind("127.0.0.1", 8000);
        for (; ;) {
            auto stream = co_await server.accept();

            cout << stream.local_address().to_string() 
                 << " connect "
                 << stream.remote_address().to_string()
                 << '\n';

            co_spawn(co_await this_coro::executor, process(std::move(stream)));
        }
    } catch(const std::runtime_error& err) {
        cout << err.what() << '\n';
    }
}

int main() {
    Magico loop(1);
    co_spawn(loop.get_executor(), amain());
    loop.run();
}
```

output

```shell
127.0.0.1:8000 connect 127.0.0.1:49699
Hello server..
Hello server..
Hello server..
Hello server..
Hello server..
EOF
```

## Benchmark

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
