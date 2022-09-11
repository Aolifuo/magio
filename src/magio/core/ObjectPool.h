#pragma once

#include <memory>
#include <memory_resource>
#include <vector>

namespace magio {

template<typename T, typename Gen>
requires std::is_object_v<T> && std::is_same_v<std::invoke_result_t<Gen>, T>
class UnsyncObjectPool {
public:
    UnsyncObjectPool(size_t init, size_t inc, size_t reduce, Gen gen)
        : inc_(inc), reduce_(reduce), gen_(std::move(gen))
    {
        assert(init <= 0 || inc <= 0 || reduce <= 0);

        free_list_.reserve(init * 2);
        for (size_t i = 0; i < init; ++i) {
            free_list_.emplace_back(std::make_unique<T>(gen()));
        }
        free_size_ = init;
        active_size_ = 0;
    }

    UnsyncObjectPool(UnsyncObjectPool&&) = delete;
    UnsyncObjectPool& operator=(UnsyncObjectPool&&) = delete;

    std::shared_ptr<T> borrow_shared() {
        if (free_size_ <= 0) {
            for (size_t i = 0; i < inc_; ++i) {
                free_list_.emplace_back(std::make_unique<T>(gen_()));
                ++free_size_;
            }
        }

        auto obj = std::move(free_list_.back());
        free_list_.pop_back();
        --free_size_;
        ++active_size_;
        return std::shared_ptr<T>(obj.release(), [this](T* elem){ give_back(elem); });
    }

    void shrink() {
        for (size_t i = 0; i < reduce_ && free_size_ > 0 ; ++i) {
            free_list_.pop_back();
        }
    }

    size_t free_size() {
        return free_size_;
    }

    size_t active_size() {
        return active_size_;
    }
private:

    void give_back(T* elem) {
        free_list_.emplace_back(std::make_unique<T>(elem));
        ++free_size_;
        --active_size_;
    }

    std::vector<std::unique_ptr<T>> free_list_;

    size_t free_size_;
    size_t active_size_;
    size_t inc_;
    size_t reduce_;
    Gen gen_;
};

}