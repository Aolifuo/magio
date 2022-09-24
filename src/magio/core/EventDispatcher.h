#pragma once

#include <unordered_map>
namespace magio {

template<typename K, typename V>
class EventDispatcher {
public:
    void on(const K& key, V value) {
        chan_.emplace(std::move(value));
    }

    void off(const K& key) {
        chan_.erase(key);
    }

    template<typename...Args>
    void dispatch(const K& key, Args&&...args) {
        if (auto iter = chan_.find(key); iter != chan_.end()) {
            iter->second(std::forward<Args>(args)...);
        }
    }
private:
    std::unordered_map<K, V> chan_;
};

}