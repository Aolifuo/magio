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

constexpr size_t kFormatBufferSize = 500;
constexpr size_t kSmallBufferSize = 4000;
constexpr size_t kLargeBufferSize = 4000 * 1000;

class WaitGroup {
public:
    WaitGroup(size_t n): wait_n_(n) 
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
        static thread_local std::string id = [] {
            std::ostringstream ss;
            ss << std::this_thread::get_id();
            return std::string(ss.str());
        }();
        return id;
    }

private:
};

template<size_t Size>
class StaticBuffer {
public:
    size_t append(std::string_view msg) {
        size_t cplen = std::min(rest(), msg.length());
        memcpy(buf_ + size_, msg.data(), cplen);
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

using FormatBuffer = StaticBuffer<kFormatBufferSize>;
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
        back_thread_ = std::thread(&AsyncLogger::run_in_background, this);
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
            current_->append(msg);
            return;
        }
        
        ready_.push_back(std::move(current_));
        if (!next_) {
            next_ = std::make_unique<LargeBuffer>();
        } 
        current_ = std::move(next_);
        current_->append(msg);

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
    WaitGroup wait_group_;

    BufferPtr current_;
    BufferPtr next_;
    BufferVector ready_;
    
    std::thread back_thread_;
};

}

enum class LogLevel {
    Off,
    Trace,
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
        Off         = 0b00000000,
        Level       = 0b00000001,
        Date        = 0b00000010,
        Time        = 0b00000100,
        File        = 0b00001000,
        Line        = 0b00010000,
        ThreadId    = 0b00100000
    };

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    static void set_output(void(* fn)(std::string_view, fmt::format_args) = nullptr) {
        ins().output_fn_ = fn;
    }

    static void set_level(LogLevel level) {
        ins().level_ = level;
    }

    static void set_pattern(LogPattern pattern) {
        ins().pattern_= pattern;
    }

    template <typename... T>
    static void write(const char* file, int line, LogLevel level, fmt::format_string<T...> fmt, T&&... args) {
        if ((int)ins().level_ > (int)level) {
            return;
        }

        build_fmt_string(file, line, level, fmt);
        ins().output_fn_(local_fmt.str_view(), fmt::make_format_args(args...));
    }

    static void default_output(std::string_view fmt, fmt::format_args args) {
        fmt::vprint(fmt, args);
    }

    static void async_output(std::string_view fmt, fmt::format_args args) {
        static detail::AsyncLogger async_logger;

        local_buffer.clear();
        local_buffer.append_format(fmt, args);
        async_logger.write(local_buffer.str_view());
    }
private:

    static void build_fmt_string(const char* file, int line, LogLevel level, fmt::string_view fmt) {
        local_fmt.clear();
        
        if (ins().pattern_ & Level) {
            switch (level) {
            case LogLevel::Trace:
                local_fmt.append("trace ");
                break;
            case LogLevel::Debug:
                local_fmt.append("debug ");
                break;
            case LogLevel::Info:
                local_fmt.append("info ");
                break;
            case LogLevel::Warn:
                local_fmt.append("warn ");
                break;
            case LogLevel::Error:
                local_fmt.append("error ");
                break;
            case LogLevel::Fatal:
                local_fmt.append("fatal ");
                break;
            default:
                break;
            }
        }

        auto date_n_time = fmt::localtime(std::chrono::system_clock::now());
        if (ins().pattern_ & Date) {
            local_fmt.append_format("{:%Y-%m-%d} ", fmt::make_format_args(date_n_time));
        }

        if (ins().pattern_ & Time) {
            local_fmt.append_format("{:%H:%M:%S} ", fmt::make_format_args(date_n_time));
        }

        if (ins().pattern_ & File) {
            local_fmt.append("f:");
            local_fmt.append(file);
            local_fmt.append(" ");
        }

        if (ins().pattern_ & Line) {
            local_fmt.append("l:");
            local_fmt.append(std::to_string(line));
            local_fmt.append(" ");
        }

        if (ins().pattern_ & ThreadId) {
            local_fmt.append("id:");
            local_fmt.append(detail::CurrentThread::get_id());
            local_fmt.append(" ");
        }

        local_fmt.append({fmt.data(), fmt.size()});
        local_fmt.append("\n");
    }

    static Logger& ins() {
        static Logger logger;
        return logger;
    }

    inline static thread_local detail::FormatBuffer local_fmt;
    inline static thread_local detail::SmallBuffer local_buffer;
    
    LogLevel level_ = LogLevel::Debug;
    int pattern_ = Level | Date | Time | File | Line | ThreadId;
    void(* output_fn_)(std::string_view, fmt::format_args) = default_output;

};

#define M_TRACE(FMT, ...) \
    do { ::magio::Logger::write(__FILE__, __LINE__, ::magio::LogLevel::Trace, FMT, __VA_ARGS__); } while(0)
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

}
