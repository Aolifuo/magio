#include "magio/magio.h"

using namespace std;
using namespace magio;

int main() {
    ThreadPool pool(8);
    
    auto a = Workflow::create_task([](ContextPtr ctx) {
        printf("task a start!\n");
    });
    auto b = Workflow::create_timed_task(5000, [](ContextPtr ctx) {
        printf("task b timeout!\n");
    });
    auto c = Workflow::create_timed_task(1000, [](ContextPtr ctx) {
        printf("task c timeout!\n");
    });
    auto d = Workflow::create_task([](ContextPtr ctx) {
        printf("task d start!\n");
    });
    auto e = Workflow::create_timed_task(2000, [](ContextPtr ctx) {
        printf("task e timeout!\n");
    });
    auto f = Workflow::create_task([](ContextPtr ctx) {
        cout << "task f start!\n";
    });

    a->successor(b, c);
    b->successor(d, e);
    c->successor(d, e);
    d->successor(f);
    e->successor(f);

    Workflow::run(pool.get_executor(), a, [](exception_ptr eptr) {
        if (eptr) {
            try {
                rethrow_exception(eptr);
            } catch(const exception& e) {
                cout << e.what() << '\n';
            }
        }
    });
}