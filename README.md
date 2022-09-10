# magio

---
magio是一个基于事件循环和线程池的协程库

## Tcp echo

### Client

```cpp
Coro<void> amain() {
    try {
        std::array<char, 1024> buf;
        auto client = co_await TcpClient::create();
        auto stream = co_await client.connect("127.0.0.1", 1234, "127.0.0.1", 8080);

        for (int i = 0; i < 5; ++i) {
            co_await stream.write("Hello server..", 14);
            size_t recv_len = co_await stream.read(buf.data(), buf.size());

            cout << string_view(buf.data(), recv_len) << '\n';
        }
    } catch(const std::runtime_error& err) {
        cout <<  err.what() << '\n';
    }
}

int main() {
    Runtime::run().unwrap();
    EventLoop loop;
    co_spawn(loop.get_executor(), amain(), detached);
    loop.run();
}
```

output

```shell
Hello client..
Hello client..
Hello client..
Hello client..
Hello client..
```

### Server

```cpp
Coro<void> process(TcpStream stream) {
    try {
        array<char, 1024> buf;
        for (; ;) {
            size_t read_len = co_await stream.read(buf.data(), buf.size());
            cout << string_view(buf.data(), read_len) << '\n';
            co_await stream.write("Hello client..", 14);
        }
    } catch(const std::runtime_error& err) {
        cout << err.what() << '\n';
    }
}

Coro<void> amain(const char* host, short port) {
    try {
        auto executor = co_await this_coro::executor;
        auto server = co_await TcpServer::bind(host, port);
        for (; ;) {
            auto stream = co_await server.accept();
            co_spawn(executor, process(std::move(stream)), detached);
        }
    } catch(const std::runtime_error& err) {
        cout << err.what() << '\n';
    }
}

int main() {
    Runtime::run().unwrap();
    EventLoop loop;
    co_spawn(loop.get_executor(), amain("127.0.0.1", 8080), detached);
    loop.run();
}
```

output
```shell
Hello server..
Hello server..
Hello server..
Hello server..
Hello server..
Client disconnected
```