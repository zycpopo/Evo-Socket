#ifndef LOOPFORGE_LOGGING_HPP
#define LOOPFORGE_LOGGING_HPP

// Use the class API: popolog.hpp also defines generic info/error macros.
#include "../third_party/popolog/logger.hpp"
#include <cerrno>
#include <stdexcept>

namespace loopforge {
using LogLevel = popolog::LogLevel::value;

struct LogConfig {
    std::string name = "loopforge";
    LogLevel level = LogLevel::DEBUG;
    bool asynchronous = true;
    bool console = true;
    std::string file_prefix = "./logs/loopforge-";
    size_t max_file_size = 10 * 1024 * 1024;
};

namespace detail {
inline popolog::Logger::ptr MakeConsoleLogger() {
    popolog::LocalLoggerBuilder builder;
    builder.buildLoggerName("loopforge");
    builder.buildLoggerLevel(LogLevel::DEBUG);
    builder.buildSink<popolog::StdoutSink>();
    return builder.build();
}

inline popolog::Logger::ptr &LoggerStorage() {
    static popolog::Logger::ptr logger = MakeConsoleLogger();
    return logger;
}
} // namespace detail

inline popolog::Logger::ptr GetLogger() {
    return std::atomic_load(&detail::LoggerStorage());
}

// Configure before starting producer threads. Failed setup preserves the logger.
inline void InitLogging(const LogConfig &config = LogConfig()) {
    if (config.name.empty() || (!config.console && config.file_prefix.empty()) ||
        (!config.file_prefix.empty() && config.max_file_size == 0)) {
        throw std::invalid_argument("invalid LoopForge logging configuration");
    }
    popolog::LocalLoggerBuilder builder;
    builder.buildLoggerName(config.name);
    builder.buildLoggerLevel(config.level);
    builder.buildLoggerType(config.asynchronous ? popolog::LoggerType::LOGGER_ASYNC
                                               : popolog::LoggerType::LOGGER_SYNC);
    builder.buildFormatter("[%d{%Y-%m-%d %H:%M:%S}][%t][%c][%f:%l][%p]%T%m%n");
    if (config.console) builder.buildSink<popolog::StdoutSink>();
    if (!config.file_prefix.empty()) {
        builder.buildSink<popolog::RollyBySizeSink>(config.file_prefix, config.max_file_size);
    }
    std::atomic_store(&detail::LoggerStorage(), builder.build());
}

// Wait for queued output and flush streams; disk fsync is not implied.
inline void FlushLogging() { GetLogger()->flush(); }

// Stop producer threads first. A synchronous console fallback remains available.
inline void ShutdownLogging() {
    auto previous = std::atomic_exchange(&detail::LoggerStorage(), detail::MakeConsoleLogger());
    previous->flush();
}

inline LogLevel LegacyLogLevel(int level) {
    switch (level) {
        case 0: return LogLevel::INFO;
        case 1: return LogLevel::DEBUG;
        case 2: return LogLevel::ERROR;
        case 3: return LogLevel::WARN;
        case 4: return LogLevel::FATAL;
        default: return LogLevel::DEBUG;
    }
}

template <typename... Args>
inline void WriteLog(const popolog::Logger::ptr &logger, LogLevel level,
                     const char *file, size_t line, const char *format, Args... args) {
    switch (level) {
        case LogLevel::DEBUG: logger->debug(file, line, format, args...); break;
        case LogLevel::INFO: logger->info(file, line, format, args...); break;
        case LogLevel::WARN: logger->warn(file, line, format, args...); break;
        case LogLevel::ERROR: logger->error(file, line, format, args...); break;
        case LogLevel::FATAL:
            logger->fatal(file, line, format, args...);
            logger->flush();
            break;
        default: break;
    }
}
} // namespace loopforge

// Keep source locations and filter before evaluating message arguments.
// Preserve errno so diagnostics cannot affect socket error handling.
#define INF 0
#define DBG 1
#define ERR 2
#define WRN 3
#define FTL 4
#define LOG(level, ...) do { \
    const int loopforge_saved_errno = errno; \
    auto loopforge_logger = ::loopforge::GetLogger(); \
    const auto loopforge_level = ::loopforge::LegacyLogLevel(level); \
    if (loopforge_logger->shouldLog(loopforge_level)) { \
        ::loopforge::WriteLog(loopforge_logger, loopforge_level, __FILE__, __LINE__, __VA_ARGS__); \
    } \
    errno = loopforge_saved_errno; \
} while (0)
#define INF_LOG(...) LOG(INF, __VA_ARGS__)
#define DBG_LOG(...) LOG(DBG, __VA_ARGS__)
#define ERR_LOG(...) LOG(ERR, __VA_ARGS__)
#define WRN_LOG(...) LOG(WRN, __VA_ARGS__)
#define FTL_LOG(...) LOG(FTL, __VA_ARGS__)

#endif
