#include "magio/Magico.h"
#include "magio/coro/Coro.h"
#include "magio/coro/CoSpawn.h"
#include <cstdio>

struct Foo {
    ~Foo() {
        std::printf("finish\n");
    }
};

magio::Coro<int> get_num() {
    co_return 10;
}

magio::Coro<void> amain() {
    auto num = co_await get_num();
    std::printf("%d\n", num);
}

int main() {
    magio::Magico loop;

    magio::co_spawn(loop.get_executor(), amain(), magio::detached);

    loop.run();
}