#pragma once

#include <string>
#include <fstream>
#include <mutex>
#include <memory>
#include <filesystem>
#include <unordered_map>

enum class LogLevel {
    INFO,
    WARNING,
    ERROR,
    DEBUG,
    WORLDGEN,
    RENDER,
    PHYSICS,
    NETWORK,
    PERFORMANCE
};

class Logger {
public:
    static Logger& getInstance();
    
    // Delete copy constructor and assignment operator
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    // Main logging functions
    void log(LogLevel level, const std::string& message);
    void logf(LogLevel level, const char* format, ...);  // Printf-style logging

    // Convenience methods for different log levels
    void info(const std::string& message) { log(LogLevel::INFO, message); }
    void warning(const std::string& message) { log(LogLevel::WARNING, message); }
    void error(const std::string& message) { log(LogLevel::ERROR, message); }
    void debug(const std::string& message) { log(LogLevel::DEBUG, message); }
    void worldgen(const std::string& message) { log(LogLevel::WORLDGEN, message); }
    void render(const std::string& message) { log(LogLevel::RENDER, message); }
    void physics(const std::string& message) { log(LogLevel::PHYSICS, message); }
    void network(const std::string& message) { log(LogLevel::NETWORK, message); }
    void performance(const std::string& message) { log(LogLevel::PERFORMANCE, message); }

    // Configuration
    void setLogDirectory(const std::string& dir);
    void setLogToConsole(bool enable) { logToConsole = enable; }
    void setLogLevel(LogLevel level, bool enabled);
    void setMaxLogSize(size_t bytes) { maxLogSize = bytes; }

private:
    Logger();
    ~Logger();

    std::string getTimestamp() const;
    std::string getStartupTimestamp() const { return startupTimestamp; }
    std::string getLevelString(LogLevel level) const;
    void rotateLogFileIfNeeded();
    void createLogDirectoryIfNeeded();
    void openLogFile();
    void initStartupTimestamp();

    std::string logDirectory;
    std::string startupTimestamp;
    std::unique_ptr<std::ofstream> logFile;
    std::unordered_map<LogLevel, bool> enabledLevels;
    bool logToConsole = true;
    size_t maxLogSize = 10 * 1024 * 1024; // 10MB default
    mutable std::mutex logMutex;
};

// Global convenience macros
#define LOG_INFO(msg) Logger::getInstance().info(msg)
#define LOG_WARNING(msg) Logger::getInstance().warning(msg)
#define LOG_ERROR(msg) Logger::getInstance().error(msg)
#define LOG_DEBUG(msg) Logger::getInstance().debug(msg)
#define LOG_WORLDGEN(msg) Logger::getInstance().worldgen(msg)
#define LOG_RENDER(msg) Logger::getInstance().render(msg)
#define LOG_PHYSICS(msg) Logger::getInstance().physics(msg)
#define LOG_NETWORK(msg) Logger::getInstance().network(msg)
#define LOG_PERF(msg) Logger::getInstance().performance(msg) 