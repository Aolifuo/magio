#pragma once

#include <exception>
#include <memory>
#include "magio/execution/Execution.h"
#include "magio/sync/Lock.h"

namespace magio {

class Context;

using ContextPtr = std::shared_ptr<Context>;
using TaskHandler = std::function<void(ContextPtr)>;

class Context {
public:
    Context(AnyExecutor exe, std::function<void(std::exception_ptr)>&& eh)
        : executor_(exe)
        , error_handler_(std::move(eh))
    {

    }

    void post(CompletionHandler&& handler) {
        executor_.post(std::move(handler));
    }

    TimerID set_timeout(size_t ms, CompletionHandler&& handler) {
        return executor_.set_timeout(ms, std::move(handler));
    }

    void handle_exception() {
        error_handler_(std::current_exception());
    }
private:
    SpinLock spin_;
    AnyExecutor executor_;
    
    std::function<void(std::exception_ptr)> error_handler_;
};

class TaskNode: public std::enable_shared_from_this<TaskNode> {

    friend class Workflow;
public:
    TaskNode(TaskHandler&& handler)
        : handler_(std::move(handler))
    {

    }

    virtual ~TaskNode() = default;

    template<typename...NodePtrs>
    void precursor(NodePtrs...nodes) {
        (nodes->successors_.emplace_back(shared_from_this()), ...);
        pre_nums_ += sizeof...(NodePtrs);
    }

    template<typename...NodePtrs>
    void successor(NodePtrs...nodes) {
        (successors_.emplace_back(nodes), ...);
        (nodes->pre_nums_++, ...);
    }

    virtual void invoke(const ContextPtr& ctx) {
        ctx->post([p = shared_from_this(), ctx] {
            try {
                p->handler_(ctx);
                p->invoke_successors(ctx);
            } catch(...) {
                ctx->handle_exception();
            }
        });
    }

protected:
    void invoke_successors(const ContextPtr& ctx) {
        for (auto& succ : successors_) {
            succ->try_invoke(ctx);
        }
    }

    void try_invoke(const ContextPtr& ctx) {
        {
            std::lock_guard lk(spin_);
            --pre_nums_;
            if (pre_nums_ != 0) {
                return;
            }
        }
        invoke(ctx);
    }

    SpinLock spin_;
    size_t pre_nums_;

    TaskHandler handler_;

    std::vector<std::shared_ptr<TaskNode>> successors_;
};

class TimedTask: public TaskNode {
public:
    TimedTask(size_t ms, TaskHandler&& handler)
        : TaskNode(std::move(handler))
        , ms_(ms)
    {

    }

    void invoke(const ContextPtr& ctx) override {
        ctx->set_timeout(ms_, [p = shared_from_this(), ctx] {
            p->TaskNode::invoke(ctx);
        });
    }
private:
    size_t ms_;
};

class Workflow {
public:
    template<typename...Fns>
    static void make_parallel(AnyExecutor executor, Fns&&...fns) {
        (executor.post([fns = std::forward<Fns>(fns)] { fns(); }), ...);
    }

    static std::shared_ptr<TimedTask> create_timed_task(size_t ms, TaskHandler&& handler) {
        return std::make_shared<TimedTask>(ms, std::move(handler));
    }

    static std::shared_ptr<TaskNode> create_task(TaskHandler&& handler) {
        return std::make_shared<TaskNode>(std::move(handler));
    }

    static void run(AnyExecutor executor, std::shared_ptr<TaskNode> node, std::function<void(std::exception_ptr)>&& error_handler) {
        auto ctx = std::make_shared<Context>(executor, std::move(error_handler));
        node->invoke(ctx);
    }
private:
};

}