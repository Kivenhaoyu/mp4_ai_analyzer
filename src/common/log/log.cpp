//
//  log.cpp
//  mp4_ai_analyzer
//
//  Created by Elena Aaron on 27/10/2025.
//

#include "log.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <ctime>

// 全局日志器初始化
Logger& Logger::getInstance() {
    static Logger instance;  // 局部静态变量，保证线程安全（C++11后）
    return instance;
}

Logger::Logger() : current_level_(LogLevel::DEBUG) {
    // 默认添加控制台输出
    addOutput(std::make_unique<ConsoleOutput>());
}

void Logger::setLogLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    current_level_ = level;
}

void Logger::addOutput(std::unique_ptr<ILogOutput> output) {
    if (output) {
        std::lock_guard<std::mutex> lock(mutex_);
        outputs_.push_back(std::move(output));
    }
}

void Logger::log(LogLevel level, const std::string& msg, const char* file, int line) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 过滤低于当前等级的日志
    if (level < current_level_) {
        return;
    }

    // 准备日志参数
    std::string time_str = get_current_time();
    std::string level_str = level_to_string(level);
    std::string file_str(file);  // 转换为std::string

    // 向所有输出目标写入日志
    for (const auto& output : outputs_) {
        output->write(time_str, level_str, file_str, line, msg);
    }

    // 致命错误：输出后终止程序
    if (level == LogLevel::FATAL) {
        std::abort();
    }
}

// 控制台输出实现
void ConsoleOutput::write(
    const std::string& time_str,
    const std::string& level_str,
    const std::string& file,
    int line,
    const std::string& msg
) {
    // 输出格式：[时间] [等级] [文件:行号] 消息
    std::cout << "[" << time_str << "] "
              << level_str << " "
              << "[" << file << ":" << line << "] "
              << msg << std::endl;
}

// 时间字符串生成
std::string get_current_time() {
    auto now = std::chrono::system_clock::now();
    auto now_t = std::chrono::system_clock::to_time_t(now);
    std::tm tm = *std::localtime(&now_t);  // 线程不安全，如需线程安全可改用localtime_r

    std::stringstream ss;
    ss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

// 日志等级转字符串
std::string level_to_string(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "[DEBUG]";
        case LogLevel::INFO:  return "[INFO] ";
        case LogLevel::WARN:  return "[WARN] ";
        case LogLevel::ERROR: return "[ERROR]";
        case LogLevel::FATAL: return "[FATAL]";
        default: return "[UNKNOWN]";
    }
}
