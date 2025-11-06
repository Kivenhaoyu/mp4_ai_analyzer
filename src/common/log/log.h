//
//  log.h
//  mp4_ai_analyzer
//
//  Created by Elena Aaron on 27/10/2025.
//

#ifndef LOG_H
#define LOG_H

#include <string>
#include <chrono>
#include <mutex>
#include <memory>
#include <vector>

// 日志等级（与原逻辑兼容，新增FATAL级别）
enum class LogLevel {
    DEBUG,   // 调试信息（开发用）
    INFO,    // 流程节点信息
    WARN,    // 警告（不影响运行）
    ERROR,   // 错误（影响运行）
    FATAL    // 致命错误（导致程序退出）
};

// 前置声明：日志输出接口（扩展点1：支持多输出目标）
class ILogOutput;

// 日志器核心类（单例模式，保证全局唯一）
class Logger {
public:
    // 获取全局唯一实例
    static Logger& getInstance();

    // 设置全局日志等级（低于该等级的日志不输出）
    void setLogLevel(LogLevel level);

    // 添加输出目标（如控制台、文件等，扩展点1）
    void addOutput(std::unique_ptr<ILogOutput> output);

    // 核心日志接口（内部调用）
    void log(LogLevel level, const std::string& msg, const char* file, int line);

    // 禁止拷贝（单例保证）
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

private:
    Logger();  // 私有构造，保证单例
    ~Logger() = default;

    LogLevel current_level_;                // 当前日志等级
    std::vector<std::unique_ptr<ILogOutput>> outputs_;  // 输出目标集合
    std::mutex mutex_;                      // 线程安全锁（扩展点2：多线程安全）
};

// 日志输出接口（扩展点：实现此接口可自定义输出目标，如文件、网络）
class ILogOutput {
public:
    virtual ~ILogOutput() = default;

    // 输出日志（参数：时间字符串、等级字符串、文件名、行号、日志内容）
    virtual void write(
        const std::string& time_str,
        const std::string& level_str,
        const std::string& file,
        int line,
        const std::string& msg
    ) = 0;
};

// 默认输出：控制台（实现ILogOutput）
class ConsoleOutput : public ILogOutput {
public:
    void write(
        const std::string& time_str,
        const std::string& level_str,
        const std::string& file,
        int line,
        const std::string& msg
    ) override;
};

// 工具函数：获取当前时间字符串（格式：YYYY-MM-DD HH:MM:SS）
std::string get_current_time();

// 工具函数：日志等级转字符串（如LogLevel::INFO -> "[INFO]"）
std::string level_to_string(LogLevel level);

// 宏定义（简化调用，自动传入文件名和行号）
#define LOG_DEBUG(msg) Logger::getInstance().log(LogLevel::DEBUG, msg, __FILE__, __LINE__)
#define LOG_INFO(msg)  Logger::getInstance().log(LogLevel::INFO,  msg, __FILE__, __LINE__)
#define LOG_WARN(msg)  Logger::getInstance().log(LogLevel::WARN,  msg, __FILE__, __LINE__)
#define LOG_ERROR(msg) Logger::getInstance().log(LogLevel::ERROR, msg, __FILE__, __LINE__)
#define LOG_FATAL(msg) Logger::getInstance().log(LogLevel::FATAL, msg, __FILE__, __LINE__)

#endif // LOG_H
