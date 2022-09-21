# magio

magio是一个基于事件循环和线程池的协程库

## Coro

```cpp
Coro<int> factorial(std::string_view name, int num) {
    int res = 1;
    Timer timer(co_await this_coro::executor, 1000);

    for (int i = 2; i <= num; ++i) {
        printf("Task %s: Compute factorial %d, now i = %d\n", name.data(), num, i);
        co_await timer.async_wait(use_coro);
        res *= i;
    }
    printf("Task %s: factorial %d = %d\n", name.data(), num, res);
    co_return res;
}

int main() {
    EventLoop loop;
    co_spawn(
        loop.get_executor(), 
        []() -> Coro<void> {
            auto [a, b, c] = co_await coro_join(
                factorial("A", 2),
                factorial("B", 3),
                factorial("C", 4)
            );
            assert(a == 2);
            assert(b == 6);
            assert(c = 24);
        }(),
        detached
    );
    loop.run();
}
```

output

```shell
Task A: Compute factorial 2, now i = 2
Task B: Compute factorial 3, now i = 2
Task C: Compute factorial 4, now i = 2
Task A: factorial 2 = 2
Task B: Compute factorial 3, now i = 3
Task C: Compute factorial 4, now i = 3
Task B: factorial 3 = 6
Task C: Compute factorial 4, now i = 4
Task C: factorial 4 = 24
```

## Channel

```cpp
void func1(shared_ptr<Channel<int, string>> chan) {
    for (size_t i = 0; i < 5; ++i) {
        chan->async_receive([](int n, string str) {
            printf("%s receive %d %s\n", "func1", n, str.c_str());
        });
    }
}

Coro<void> func2(shared_ptr<Channel<int, string>> chan) {
    for (size_t i = 0; i < 5; ++i) {    
        auto [n, str] = co_await chan->async_receive(use_coro);
        printf("%s receive %d %s\n", "func2", n, str.c_str());
    }
}

int main() {
    ThreadPool pool(10);

    auto channel = make_shared<Channel<int, string>>(pool.get_executor());
    for (size_t i = 0; i < 10; ++i) {
        channel->async_send(i, "from main");
    }
    pool.post([=] { func1(channel); });
    co_spawn(pool.get_executor(), func2(channel), detached);
    
    pool.join();
}
```

output

```shell
func1 receive 0 from main
func1 receive 1 from main
func1 receive 2 from main
func1 receive 3 from main
func1 receive 4 from main
func2 receive 5 from main
func2 receive 6 from main
func2 receive 7 from main
func2 receive 8 from main
func2 receive 9 from main
```

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
