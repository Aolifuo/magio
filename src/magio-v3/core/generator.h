#ifndef MAGIO_CORE_GENERATOR_H_
#define MAGIO_CORE_GENERATOR_H_

#include <utility>
#include <iterator>
#include <exception>

#include "magio-v3/core/coroutine.h"

namespace magio {

template<typename Ref, typename Value = std::remove_cvref_t<Ref>>
class Generator {
public:
    class promise_type {
    public:
        promise_type() : root_(this) {}
        Generator get_return_object() noexcept {
            return Generator{std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        void unhandled_exception() {
            if (exception_ == nullptr)
                throw;
            *exception_ = std::current_exception();
        }

        void return_void() noexcept {}

        std::suspend_always initial_suspend() noexcept { return {}; }

        // Transfers control back to the parent of a nested coroutine
        struct final_awaiter {
            bool await_ready() noexcept {
                return false;
            }
            std::coroutine_handle<> await_suspend(
                std::coroutine_handle<promise_type> h) noexcept {
                auto& promise  = h.promise();
                auto  parent   = h.promise().parent_;
                if (parent) {
                    promise.root_->leaf_ = parent;
                    return std::coroutine_handle<promise_type>::from_promise(*parent);
                }
                return std::noop_coroutine();
            }
            void await_resume() noexcept {}
        };

        final_awaiter final_suspend() noexcept { return {}; }

        std::suspend_always yield_value(Ref&& x) noexcept {
            root_->value_ = std::addressof(x);
            return {};
        }
        std::suspend_always yield_value(Ref& x) noexcept {
            root_->value_ = std::addressof(x);
            return {};
        }

        struct yield_sequence_awaiter {
            Generator gen_;
            std::exception_ptr exception_;

            explicit yield_sequence_awaiter(Generator&& g) noexcept
            // Taking ownership of the Generator ensures frame are destroyed 
            // in the reverse order of their creation
            : gen_(std::move(g))
            {}

            bool await_ready() noexcept {
                return !gen_.coro_;
            }

            // set the parent, root and exceptions pointer and
            // resume the nested coroutine
            std::coroutine_handle<> await_suspend(
                std::coroutine_handle<promise_type> h) noexcept {
                auto& current = h.promise();
                auto& nested  = gen_.coro_.promise();
                auto& root    = current.root_;
                
                nested.root_   = root;
                root->leaf_    = &nested;
                nested.parent_ = &current;

                nested.exception_  = &exception_;
                
                // Immediately resume the nested coroutine (nested Generator)
                return gen_.coro_;
            }

            void await_resume() {
                if (exception_) {
                    std::rethrow_exception(exception_);
                }
            }
        };

        yield_sequence_awaiter yield_value(Generator&& g) noexcept {
            return yield_sequence_awaiter{std::move(g)};
        }

        void resume() {
            std::coroutine_handle<promise_type>::from_promise(*leaf_).resume();
        }

        // Disable use of co_await within this coroutine.
        void await_transform() = delete;

    private:
        friend Generator;
        
        // Technically UB, for demonstration purpose
        union {
            promise_type* root_;
            promise_type* leaf_;
        };
        promise_type* parent_ = nullptr;
        std::exception_ptr* exception_ = nullptr;
        std::add_pointer_t<Ref> value_;
    };

    Generator() noexcept = default;

    Generator(Generator&& other) noexcept
    : coro_(std::exchange(other.coro_, {}))
    {}

    ~Generator() noexcept {
        if (coro_) {
            coro_.destroy();
        }
    }

    Generator& operator=(Generator g) noexcept {
        swap(g);
        return *this;
    }

    void swap(Generator& other) noexcept {
        std::swap(coro_, other.coro_);
    }

    struct sentinel {};

    class iterator {
        using coroutine_handle = std::coroutine_handle<promise_type>;

      public:
        using iterator_category = std::input_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = Value;
        using reference =  Ref;
        using pointer =    std::add_pointer_t<Ref>;

        iterator() noexcept = default;
        iterator(const iterator &) = delete;
        iterator(iterator && o) noexcept {
            std::swap(coro_, o.coro_);
        }

        iterator &operator=(iterator && o) noexcept {
            std::swap(coro_, o.coro_);
            return *this;
        }

        ~iterator() {}

        friend bool operator==(const iterator &it, sentinel) noexcept {
            return !it.coro_ || it.coro_.done();
        }

        iterator &operator++() {
            coro_.promise().resume();
            return *this;
        }
        void operator++(int) {
            (void)operator++();
        }

        reference operator*() const noexcept {
            return static_cast<reference>(*coro_.promise().value_);
        }

        pointer operator->() const noexcept 
        requires std::is_reference_v<reference> {
            return std::addressof(operator*());
        }

      private:
        friend Generator;

        explicit iterator(coroutine_handle coro) noexcept
            : coro_(coro) {}

        coroutine_handle coro_;
    };

    iterator begin() {
        if (coro_) {
            coro_.resume();
        }
        return iterator{coro_};
    }

    sentinel end() noexcept {
        return {};
    }

private:
    explicit Generator(std::coroutine_handle<promise_type> coro) noexcept
    : coro_(coro)
    {}

    std::coroutine_handle<promise_type> coro_;
};

}

#endif