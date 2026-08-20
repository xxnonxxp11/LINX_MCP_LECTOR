#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <vector>
#include <mutex>
#include <deque>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <android/log.h>

enum class LogLevel {
    DEBUG_LVL = 0,
    INFO_LVL  = 1,
    WARN_LVL  = 2,
    ERROR_LVL = 3,
    CRIT_LVL  = 4
};

struct LogEntry {
    uint64_t timestamp_ms;
    std::string time_str;
    LogLevel level;
    std::string category;
    std::string message;
    std::string file;
    int line;
    int tid;
    int err_code;
    std::string err_desc;
};

class Logger {
public:
    static Logger& getInstance() {
        static Logger instance;
        return instance;
    }

    void init(const std::string& log_file_path = "/data/local/tmp/mem_server_debug.log", size_t max_in_memory = 2000) {
        std::lock_guard<std::mutex> lock(mtx);
        this->log_path = log_file_path;
        this->max_entries = max_in_memory;
        
        file_handle = fopen(log_path.c_str(), "a");
        if (file_handle) {
            fprintf(file_handle, "\n\n=======================================================\n");
            fprintf(file_handle, "=== [MEM_SERVER ULTRA-DIAGNOSTIC SESSION START] ===\n");
            fprintf(file_handle, "=======================================================\n");
            fflush(file_handle);
        }
    }

    void log(LogLevel level, const char* category, const char* file, int line, const char* fmt, ...) {
        int saved_errno = errno;
        char buffer[4096];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buffer, sizeof(buffer), fmt, args);
        va_end(args);

        auto now = std::chrono::system_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

        time_t tt = std::chrono::system_clock::to_time_t(now);
        tm local_tm;
        localtime_r(&tt, &local_tm);
        char time_buf[64];
        snprintf(time_buf, sizeof(time_buf), "%02d:%02d:%02d.%03d",
                 local_tm.tm_hour, local_tm.tm_min, local_tm.tm_sec, (int)(ms % 1000));

        // Extract filename from full path
        const char* slash = strrchr(file, '/');
        const char* filename = slash ? slash + 1 : file;

        int tid = (int)syscall(SYS_gettid);

        std::string err_desc;
        if (saved_errno != 0 && (level == LogLevel::ERROR_LVL || level == LogLevel::CRIT_LVL || level == LogLevel::WARN_LVL)) {
            err_desc = strerror(saved_errno);
        }

        const char* lvl_str = "INFO";
        int android_lvl = ANDROID_LOG_INFO;
        switch (level) {
            case LogLevel::DEBUG_LVL: lvl_str = "DEBUG"; android_lvl = ANDROID_LOG_DEBUG; break;
            case LogLevel::INFO_LVL:  lvl_str = "INFO";  android_lvl = ANDROID_LOG_INFO;  break;
            case LogLevel::WARN_LVL:  lvl_str = "WARN";  android_lvl = ANDROID_LOG_WARN;  break;
            case LogLevel::ERROR_LVL: lvl_str = "ERROR"; android_lvl = ANDROID_LOG_ERROR; break;
            case LogLevel::CRIT_LVL:  lvl_str = "FATAL"; android_lvl = ANDROID_LOG_FATAL; break;
        }

        // 1. Android logcat
        if (!err_desc.empty()) {
            __android_log_print(android_lvl, "MemServer", "[%s][%s][TID:%d] %s [errno %d: %s] (%s:%d)",
                                lvl_str, category, tid, buffer, saved_errno, err_desc.c_str(), filename, line);
        } else {
            __android_log_print(android_lvl, "MemServer", "[%s][%s][TID:%d] %s (%s:%d)",
                                lvl_str, category, tid, buffer, filename, line);
        }

        // 2. Terminal stdout
        const char* color_code = "\033[0m";
        if (level == LogLevel::ERROR_LVL || level == LogLevel::CRIT_LVL) color_code = "\033[1;31m";
        else if (level == LogLevel::WARN_LVL) color_code = "\033[1;33m";
        else if (level == LogLevel::DEBUG_LVL) color_code = "\033[1;36m";
        else color_code = "\033[1;32m";

        if (!err_desc.empty()) {
            printf("%s[%s][%s][%s][T:%d] %s \033[1;35m(errno %d: %s)\033[0m%s (%s:%d)\n",
                   color_code, time_buf, lvl_str, category, tid, buffer, saved_errno, err_desc.c_str(), color_code, filename, line);
        } else {
            printf("%s[%s][%s][%s][T:%d] %s\033[0m (%s:%d)\n",
                   color_code, time_buf, lvl_str, category, tid, buffer, filename, line);
        }
        fflush(stdout);

        // 3. Thread-safe log buffer & persistent file
        std::lock_guard<std::mutex> lock(mtx);
        if (file_handle) {
            if (!err_desc.empty()) {
                fprintf(file_handle, "[%s][%s][%s][T:%d] %s (errno %d: %s) (%s:%d)\n",
                        time_buf, lvl_str, category, tid, buffer, saved_errno, err_desc.c_str(), filename, line);
            } else {
                fprintf(file_handle, "[%s][%s][%s][T:%d] %s (%s:%d)\n",
                        time_buf, lvl_str, category, tid, buffer, filename, line);
            }
            fflush(file_handle);
        }

        LogEntry entry;
        entry.timestamp_ms = ms;
        entry.time_str = time_buf;
        entry.level = level;
        entry.category = category;
        entry.message = buffer;
        entry.file = filename;
        entry.line = line;
        entry.tid = tid;
        entry.err_code = saved_errno;
        entry.err_desc = err_desc;

        entries.push_back(entry);
        if (entries.size() > max_entries) {
            entries.pop_front();
        }
    }

    void log_hex(LogLevel level, const char* category, const char* file, int line, const char* tag, const void* data, size_t size) {
        if (!data || size == 0) return;
        const uint8_t* p = (const uint8_t*)data;
        size_t print_len = size > 64 ? 64 : size;
        char hex_buf[256];
        int pos = 0;
        for (size_t i = 0; i < print_len; i++) {
            pos += snprintf(hex_buf + pos, sizeof(hex_buf) - pos, "%02X ", p[i]);
        }
        if (size > 64) {
            snprintf(hex_buf + pos, sizeof(hex_buf) - pos, "... (%zu bytes total)", size);
        }
        log(level, category, file, line, "%s [Hex dump %zu bytes]: %s", tag, size, hex_buf);
    }

    std::vector<LogEntry> get_logs(size_t limit = 200, LogLevel min_level = LogLevel::DEBUG_LVL) {
        std::lock_guard<std::mutex> lock(mtx);
        std::vector<LogEntry> out;
        for (auto it = entries.rbegin(); it != entries.rend() && out.size() < limit; ++it) {
            if (it->level >= min_level) {
                out.push_back(*it);
            }
        }
        return out;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mtx);
        entries.clear();
        if (file_handle) {
            fclose(file_handle);
            file_handle = fopen(log_path.c_str(), "w");
            if (file_handle) fflush(file_handle);
        }
    }

private:
    Logger() : file_handle(nullptr), max_entries(2000), log_path("/data/local/tmp/mem_server_debug.log") {}
    ~Logger() {
        if (file_handle) fclose(file_handle);
    }

    std::mutex mtx;
    FILE* file_handle;
    size_t max_entries;
    std::string log_path;
    std::deque<LogEntry> entries;
};

// Convenience logging macros
#define LOG_DEBUG(cat, fmt, ...) Logger::getInstance().log(LogLevel::DEBUG_LVL, cat, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_INFO(cat, fmt, ...)  Logger::getInstance().log(LogLevel::INFO_LVL,  cat, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_WARN(cat, fmt, ...)  Logger::getInstance().log(LogLevel::WARN_LVL,  cat, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_ERROR(cat, fmt, ...) Logger::getInstance().log(LogLevel::ERROR_LVL, cat, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_CRIT(cat, fmt, ...)  Logger::getInstance().log(LogLevel::CRIT_LVL,  cat, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_FATAL(cat, fmt, ...) Logger::getInstance().log(LogLevel::CRIT_LVL,  cat, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_HEX(cat, tag, data, size) Logger::getInstance().log_hex(LogLevel::DEBUG_LVL, cat, __FILE__, __LINE__, tag, data, size)

#endif // LOGGER_H
