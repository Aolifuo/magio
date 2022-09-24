#pragma once

#include <exception>
#include <memory>
#include "magio/core/Execution.h"
#include "magio/sync/Lock.h"

namespace magio {

class TaskNode: public std::enable_shared_from_this<TaskNode> {

    friend class Workflow;
public:
    TaskNode(CompletionHandler&& handler)
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

    virtual void invoke(AnyExecutor executor) {
        handler_();
        invoke_successors(executor);
    }

protected:
    void invoke_successors(AnyExecutor executor) {
        for (auto& succ : successors_) {
            succ->try_invoke(executor);
        }
    }

    void try_invoke(AnyExecutor executor) {
        std::lock_guard lk(spin_);
        --pre_nums_;
        if (pre_nums_ == 0) {
            executor.post([p = shared_from_this(), executor] {
                p->invoke(executor);
            });
        }
    }

    SpinLock spin_;
    size_t pre_nums_;

    CompletionHandler handler_;
    std::exception_ptr eptr_;

    std::vector<std::shared_ptr<TaskNode>> successors_;
};

class TimedTask: public TaskNode {
public:
    TimedTask(size_t ms, CompletionHandler&& handler)
        : TaskNode(std::move(handler))
        , ms_(ms)
    {

    }

    void invoke(AnyExecutor executor) override {
        executor.set_timeout(ms_, [p = shared_from_this(), executor] {
            p->TaskNode::invoke(executor);
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

    static std::shared_ptr<TimedTask> create_timed_task(size_t ms, CompletionHandler&& handler) {
        return std::make_shared<TimedTask>(ms, std::move(handler));
    }

    static std::shared_ptr<TaskNode> create_task(CompletionHandler&& handler) {
        return std::make_shared<TaskNode>(std::move(handler));
    }

    static void run(AnyExecutor executor, std::shared_ptr<TaskNode> node) {
        node->invoke(executor);
        if (node->eptr_) {
            std::rethrow_exception(node->eptr_);
        }
    }
private:
};

}