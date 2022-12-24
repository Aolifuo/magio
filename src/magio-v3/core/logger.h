#ifndef MAGIO_CORE_LOGGER_H_
#define MAGIO_CORE_LOGGER_H_

#include "magio-v3/core/buffer.h"
#include "magio-v3/core/current_thread.h"

#include "fmt/chrono.h"

namespace magio {

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

        build_fmt_str(file, line, level, fmt);
        ins().output_fn_(local_fmt.slice(), fmt::make_format_args(args...));
    }

    template <typename... T>
    static void console_write(const char* file, int line, LogLevel level, fmt::format_string<T...> fmt, T&&... args) {
        if ((int)ins().level_ > (int)level) {
            return;
        }

        build_fmt_str(file, line, level, fmt);
        fmt::vprint(local_fmt.slice(), fmt::make_format_args(args...));
    }

    static void default_output(std::string_view fmt, fmt::format_args args) {
        fmt::vprint(fmt, args);
    }

private:
    static void build_fmt_str(const char* file, int line, LogLevel level, fmt::string_view fmt) {
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
            local_fmt.append(std::to_string(CurrentThread::get_id()));
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
    do { ::magio::Logger::console_write(__FILE__, __LINE__, ::magio::LogLevel::Fatal, FMT, __VA_ARGS__); std::terminate(); } while(0)
#define M_SYS_ERROR(FMT, ...) \
    do { ::magio::Logger::console_write(__FILE__, __LINE__, ::magio::LogLevel::Error, FMT, __VA_ARGS__); } while(0)
}

#endif