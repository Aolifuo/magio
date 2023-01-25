#ifndef MAGIO_UTILS_ASYNC_LOGGER_H
#define MAGIO_UTILS_ASYNC_LOGGER_H

#include <memory>
#include <vector>

#include "fmt/os.h"
#include "fmt/chrono.h"

#include "magio-v3/utils/buffer.h"
#include "magio-v3/utils/noncopyable.h"
#include "magio-v3/utils/wait_group.h"

namespace magio {

class AsyncLogger: Noncopyable {
    using BufferPtr = std::unique_ptr<detail::LargeBuffer>;
    using BufferVector = std::vector<BufferPtr>;

    AsyncLogger()
        : file_id_(0)
        , stop_flag_(false)
        , wait_group_(1)
        , current_(std::make_unique<detail::LargeBuffer>())
        , next_(std::make_unique<detail::LargeBuffer>())
    {
        back_thread_ = std::thread(&AsyncLogger::run_in_background, this);
        wait_group_.wait();
    }

public:
    ~AsyncLogger() {
        {
            std::lock_guard lk(mutex_);
            stop_flag_ = true;
        }
        condvar_.notify_all();

        if (back_thread_.joinable()) {
            back_thread_.join();
        }
    }

    static AsyncLogger& ins() {
        static AsyncLogger logger;
        return logger;
    }

    static void write(std::string_view fmt, fmt::format_args args) {
        local_buffer.clear();
        local_buffer.append_format(fmt, args);
        ins().write_impl(local_buffer.slice());
    }

private:
    void write_impl(std::string_view msg) {
        std::lock_guard lk(ins().mutex_);
        if (current_->rest() > msg.length()) {
            current_->append(msg);
            return;
        }
        
        ready_.push_back(std::move(current_));
        if (!next_) {
            next_ = std::make_unique<detail::LargeBuffer>();
        } 
        current_.swap(next_);
        current_->append(msg);

        condvar_.notify_one();
    }

    fmt::file open_file() {
        auto str = fmt::format(
            "magio_{:%Y-%m-%d_%H.%M.%S}_log_{}", 
            std::chrono::system_clock::now(), 
            ++file_id_
        );
        return {str.c_str(), fmt::file::WRONLY | fmt::file::CREATE | fmt::file::TRUNC};
    }
    
    void run_in_background() {
        using namespace std::chrono_literals;
        wait_group_.done();
        BufferPtr current_alter_buf = std::make_unique<detail::LargeBuffer>();
        BufferPtr next_alter_buf = std::make_unique<detail::LargeBuffer>();;
        BufferVector buffers_to_print;

        auto record_time = std::chrono::system_clock::now();
        auto one_day = std::chrono::hours(24);
        fmt::file file = open_file();
        
        for (; ;) {
            auto dif = std::chrono::system_clock::now() - record_time;
            if (dif >= one_day) {
                file.close();
                file = open_file();
                record_time = std::chrono::system_clock::now();
            }

            // TODO shrink
            buffers_to_print.clear();
            {
                std::unique_lock lk(mutex_);
                condvar_.wait_for(lk, 3s);

                if (ready_.empty()) {
                    if (current_->size() != 0) {
                        buffers_to_print.push_back(std::move(current_));
                        current_.swap(current_alter_buf);
                    }
                } else {
                    buffers_to_print.swap(ready_);
                }

                if (!next_) {
                    next_.swap(next_alter_buf);
                }
            }

            for (auto& ptr : buffers_to_print) {
                auto str = ptr->slice();
                file.write(str.data(), str.length());
                ptr->clear();
            }

            if (!current_alter_buf) {
                current_alter_buf.swap(buffers_to_print.back());
                buffers_to_print.pop_back();
            }
            if (!next_alter_buf) {
                next_alter_buf.swap(buffers_to_print.back());
                buffers_to_print.pop_back();
            }

            if (stop_flag_) {
                return;
            }
        }
    }

    size_t file_id_;
    bool stop_flag_;

    std::mutex mutex_;
    std::condition_variable condvar_;
    WaitGroup wait_group_;

    BufferPtr current_;
    BufferPtr next_;
    BufferVector ready_;
    
    std::thread back_thread_;

    inline static thread_local detail::SmallBuffer local_buffer;
};

}

#endif