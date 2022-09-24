#pragma once

#include <cassert>
#include <chrono>
#include <exception>
#include <mutex>
#include <string>
#include <vector>
#include <concepts>
#include <ranges>
#include <variant>
#include <random>
#include <functional>

#include "magio/core/Log.h"
#include "magio/core/Error.h"

#define TESTCASE(cases...) \
    do { \
        return collect(__FUNCTION__, {cases}); \
    } while(0)

#define TESTFAIL(MSG) \
    do { \
        return collect(__FUNCTION__, {{TestError{MSG}}}); \
    } while(0)

#define TESTSUCCESS() \
    do { \
        return collect(__FUNCTION__, {}); \
    } while(0)

#define TESTALL(...) \
    do { \
        auto results = collect_test_results(__VA_ARGS__); \
        is_all_ok_or_throw(results); \
        LOG("Ok\n"); \
    } while(0)


using namespace std;
using namespace magio;

struct NoCopy {
    NoCopy() = default;
    NoCopy(const NoCopy&) = delete;
    NoCopy(NoCopy &&) = default;

    NoCopy& operator=(NoCopy&&) = default;

    void operator()() const {

    }
};

struct TestError {
    std::string msg;
};

struct TestResults {
    std::string test_name;
    std::vector<Expected<Unit, TestError>> results;
};


template<typename L, typename R>
requires std::equality_comparable_with<L, R>
inline Expected<Unit, TestError> test(L left, R right, std::string err = "") {
    if (left != right) {
        return {TestError{std::format("{}\nleft:\n{}\nright:\n{}\n", err, left, right)}};
    }
    return {Unit()};
}

inline Expected<Unit, TestError> test(bool flag, std::string err = "") {
    if (!flag) {
        return {TestError{std::format("{}\n", err)}};
    }
    return {Unit()};
}


inline TestResults collect(std::string_view name, std::initializer_list<Expected<Unit, TestError>> li) {
    TestResults res;
    res.test_name = name;
    for (auto&& elem: li) {
        res.results.emplace_back(const_cast<Expected<Unit, TestError>&&>(elem));
    }
    return res;
}

template<typename...Args>
inline std::vector<TestResults> collect_test_results(Args&&...args) {
    std::vector<TestResults> results;
    (results.emplace_back(std::forward<Args>(args)), ...);
    return results;
}

inline void is_all_ok_or_throw(std::vector<TestResults>& results) {
    bool flag = true;
    for (auto& test_res : results) {
        for (auto& res : test_res.results) {
            if (!res) {
                flag = false;
                LOG("{}: {}\n", test_res.test_name, res.unwrap_err().msg);
            }
        }
    }

    if (!flag) {
        throw std::exception("Test failed");
    }
}

inline std::string build_string(std::initializer_list<std::string> li) {
    std::string res;
    for (auto& str : li) {
        res.append(str);
    }
    return res;
}


class TestTimer {
public:
    TestTimer(std::string msg): msg_(std::move(msg)) {}

    template <class Rep, class Period>
    void limit(const chrono::duration<Rep, Period>& time) {
        flag = false;
        thread th([this, time] {
            std::unique_lock lk(m_);
            cv_.wait_for(lk, time);
            if (flag) {
                return;
            }
            LOG("{}\n", msg_);
            std::terminate();
        });
        
        th.detach();
    }

    void clear() {
        flag = true;
        cv_.notify_one();
    }
private:
    std::string msg_;
    std::mutex m_;
    std::condition_variable cv_;
    bool flag = false;
};
