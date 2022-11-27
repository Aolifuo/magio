#pragma once

#include <mutex>
#include <thread>
#include <vector>
#include <memory>
#include <cassert>
#include <sstream>

#include "fmt/chrono.h"
#include "fmt/os.h"

namespace magio {

namespace detail {

constexpr size_t kSmallBufferSize = 1000;
constexpr size_t kLargeBufferSize = 2000 * 1000;

class _WaitGroup {
public:
    _WaitGroup(size_t n): wait_n_(n) 
    { }

    void wait() {
        std::unique_lock lk(m_);
        cv_.wait(lk, [this] {
            return wait_n_ == 0;
        });
    }

    void done() {
        std::lock_guard lk(m_);
        --wait_n_;
        if (wait_n_ == 0) {
            cv_.notify_all();
        }
    }
private:
    size_t wait_n_;
    std::mutex m_;
    std::condition_variable cv_;
};

class CurrentThread {
public:
    static std::string_view get_id() {
        static thread_local std::string id = (std::ostringstream{} << std::this_thread::get_id()).str();
        return id;
    }

private:
};

template<size_t Size>
class StaticBuffer {
public:
    size_t append(const char* ptr, size_t len) {
        size_t cplen = std::min(rest(), len);
        memcpy(buf_ + size_, ptr, cplen);
        size_ += cplen;
        return cplen;
    }

    size_t append_format(std::string_view fmt, fmt::format_args args) {
        auto sv = fmt::vformat_to_n(buf_ + size_, rest(), fmt, args);
        size_ += sv.size;
        return sv.size;
    }

    size_t rest() {
        return Size - size_;
    }

    void clear() {
        size_ = 0;
    }
    
    size_t size() {
        return size_;
    }

    std::string_view str_view() {
        return {buf_, size_};
    }

private:
    char buf_[Size];
    size_t size_ = 0;
};

using SmallBuffer = StaticBuffer<kSmallBufferSize>;
using LargeBuffer = StaticBuffer<kLargeBufferSize>;

class LogFile {
public:
    LogFile(std::string_view prefix)
        : prefix_(prefix) 
    { }

    LogFile(const LogFile&) = delete;
    LogFile& operator=(const LogFile&) = delete;

    ~LogFile() {
        
    }

    void open() {
        std::string file_name 
            = prefix_ 
            + fmt::format("{:%Y-%m-%d_%H.%M.%S}_", std::chrono::system_clock::now())
            + "log";
        out_file_ = std::make_unique<fmt::ostream>(fmt::output_file(file_name));
    }

    void close() {
        out_file_->close();
        out_file_.reset();
    }
    
    void write(std::string_view str) {
        out_file_->print("{}", str);
    }

    void flush() {
        out_file_->flush();
    }

private:
    std::string prefix_;
    std::unique_ptr<fmt::ostream> out_file_;
};


class AsyncLogger {
    using BufferPtr = std::unique_ptr<LargeBuffer>;
    using BufferVector = std::vector<BufferPtr>;

public:
    AsyncLogger()
        : stop_flag_(false)
        , wait_group_(1)
        , current_(std::make_unique<LargeBuffer>())
        , next_(std::make_unique<LargeBuffer>())
    {
        std::exchange(back_thread_, std::thread(&AsyncLogger::run_in_background, this));
        wait_group_.wait();
    }

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

    void write(std::string_view msg) {
        std::lock_guard lk(mutex_);

        if (current_->rest() > msg.length()) {
            current_->append(msg.data(), msg.length());
            return;
        }
        
        ready_.push_back(std::move(current_));
        if (!next_) {
            next_ = std::make_unique<LargeBuffer>();
        } 
        current_ = std::move(next_);
        current_->append(msg.data(), msg.length());

        condvar_.notify_one();
    }

private:
    void run_in_background() {
        using namespace std::chrono_literals;

        wait_group_.done();
        BufferPtr current_alter_buf = std::make_unique<LargeBuffer>();
        BufferPtr next_alter_buf = std::make_unique<LargeBuffer>();;
        BufferVector buffer_to_print;
        LogFile file("magio_");

        file.open();
        for (; ;) {
            buffer_to_print.clear();

            {
                std::unique_lock lk(mutex_);

                condvar_.wait_for(lk, 3s);

                buffer_to_print.swap(ready_);
                buffer_to_print.push_back(std::move(current_));
                current_ = std::move(current_alter_buf);
                if (!next_) {
                    next_ = std::move(next_alter_buf);
                }
            }

            // buffer_to_print.size() >= 1
            for (auto& ptr : buffer_to_print) {
                file.write(ptr->str_view());
                ptr->clear();
            }
            
            file.flush();

            current_alter_buf = std::move(buffer_to_print.back());
            buffer_to_print.pop_back();
            if (!next_alter_buf) {
                assert(!buffer_to_print.empty());
                next_alter_buf = std::move(buffer_to_print.back());
            }

            if (stop_flag_) {
                return;
            }
        }
    }

    bool stop_flag_;
    std::mutex mutex_;
    std::condition_variable condvar_;
    _WaitGroup wait_group_;

    BufferPtr current_;
    BufferPtr next_;
    BufferVector ready_;
    
    std::thread back_thread_;
};

}

enum class LogLevel {
    Debug,
    Info,
    Warn,
    Error,
    Fatal
};

class Logger {
    Logger() = default;

public:
    enum LogPattern {
        Date        = 0b00000001,
        Time        = 0b00000010,
        File        = 0b00000100,
        Line        = 0b00001000,
        ThreadId    = 0b00010000
    };

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    static void init_async_logger() {
        ins().alog_ = std::make_unique<detail::AsyncLogger>();
    }

    static void set_level(LogLevel level) {
        ins().level_ = level;
    }

    static void set_pattern(LogPattern pattern) {
        ins().pattern_= pattern;
    }

    template <typename... T>
    static void async_write(const char* file, int line, LogLevel level, fmt::format_string<T...> fmt, T&&... args) {
        if ((int)ins().level_ > (int)level || !ins().alog_) {
            return;
        }

        std::string fmt_str = build_fmt_str(file, line, level, fmt);
        local_buffer.clear();
        local_buffer.append_format(fmt_str, fmt::make_format_args(args...));
        ins().alog_->write(local_buffer.str_view());
    }

    template <typename... T>
    static void write(const char* file, int line, LogLevel level, fmt::format_string<T...> fmt, T&&... args) {
        if ((int)ins().level_ > (int)level) {
            return;
        }

        std::string fmt_str = build_fmt_str(file, line, level, fmt);
        fmt::vprint(fmt_str, fmt::make_format_args(args...));
    }

private:
    static std::string build_fmt_str(const char* file, int line, LogLevel level, fmt::string_view fmt) {
        std::string fmt_str;

        switch (level) {
        case LogLevel::Debug:
            fmt_str.append("debug ");
            break;
        case LogLevel::Info:
            fmt_str.append("info ");
            break;
        case LogLevel::Warn:
            fmt_str.append("warn ");
            break;
        case LogLevel::Error:
            fmt_str.append("error ");
            break;
        case LogLevel::Fatal:
            fmt_str.append("fatal ");
            break;
        default:
            break;
        }

        auto date_n_ = fmt::localtime(std::chrono::system_clock::now());
        if (ins().pattern_ & Date) {
            fmt_str.append(fmt::format("{:%Y-%m-%d} ", date_n_));
        }

        if (ins().pattern_ & Time) {
            fmt_str.append(fmt::format("{:%H:%M:%S} ", date_n_));
        }

        if (ins().pattern_ & File) {
            fmt_str.append("f:");
            fmt_str.append(file);
            fmt_str.append(" ");
        }

        if (ins().pattern_ & Line) {
            fmt_str.append("l:");
            fmt_str.append(std::to_string(line));
            fmt_str.append(" ");
        }

        if (ins().pattern_ & ThreadId) {
            fmt_str.append("id:");
            fmt_str.append(detail::CurrentThread::get_id());
            fmt_str.append(" ");
        }

        fmt_str.append(fmt.data(), fmt.size());
        fmt_str.append("\n");

        return fmt_str;
    }

    static Logger& ins() {
        static Logger logger;
        return logger;
    }

    inline static thread_local detail::SmallBuffer local_buffer;
    
    LogLevel level_ = LogLevel::Debug;
    int pattern_ = Date | Time | File | Line | ThreadId;

    std::unique_ptr<detail::AsyncLogger> alog_;
};

#define M_DEBUG(FMT, ...) \
    do { ::magio::Logger::write(__FILE__, __LINE__, ::magio::LogLevel::Debug, FMT, __VA_ARGS__); } while(0)
#define M_INFO(FMT, ...) \
    do { ::magio::Logger::write(__FILE__, __LINE__, ::magio::LogLevel::Info, FMT, __VA_ARGS__); } while(0)
#define M_WARN(FMT, ...) \
    do { ::magio::Logger::write(__FILE__, __LINE__, ::magio::LogLevel::Warn, FMT, __VA_ARGS__); } while(0)
#define M_ERROR(FMT, ...) \
    do { ::magio::Logger::write(__FILE__, __LINE__, ::magio::LogLevel::Error, FMT, __VA_ARGS__); } while(0)
#define M_FATAL(FMT, ...) \
    do { ::magio::Logger::write(__FILE__, __LINE__, ::magio::LogLevel::Fatal, FMT, __VA_ARGS__); std::terminate(); } while(0)


#define ASYNC_DEBUG(FMT, ...) \
    do { ::magio::Logger::async_write(__FILE__, __LINE__, ::magio::LogLevel::Debug, FMT, __VA_ARGS__); } while(0)
#define ASYNC_INFO(FMT, ...) \
    do { ::magio::Logger::async_write(__FILE__, __LINE__, ::magio::LogLevel::Info, FMT, __VA_ARGS__); } while(0)
#define ASYNC_WARN(FMT, ...) \
    do { ::magio::Logger::async_write(__FILE__, __LINE__, ::magio::LogLevel::Warn, FMT, __VA_ARGS__); } while(0)
#define ASYNC_ERROR(FMT, ...) \
    do { ::magio::Logger::async_write(__FILE__, __LINE__, ::magio::LogLevel::Error, FMT, __VA_ARGS__); } while(0)
}
