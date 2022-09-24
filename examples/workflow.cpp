#include <iostream>
#include "magio/workflow/Workflow.h"
#include "magio/ThreadPool.h"

using namespace std;
using namespace magio;


int main() {
    ThreadPool pool(10);
    
    auto a = Workflow::create_task([] {
        printf("task a start!\n");
    });
    auto b = Workflow::create_timed_task(5000, []{
        printf("task b timeout!\n");
    });
    auto c = Workflow::create_timed_task(1000, [] {
        printf("task c timeout!\n");
    });
    auto d = Workflow::create_task([] {
        printf("task d start!\n");
    });
    auto e = Workflow::create_timed_task(2000, [] {
        printf("task e timeout!\n");
    });
    auto f = Workflow::create_task([] {
        cout << "task f start!\n";
    });

    a->successor(b, c);
    b->successor(d, e);
    c->successor(d, e);
    d->successor(f);
    e->successor(f);

    Workflow::run(pool.get_executor(), a);
}