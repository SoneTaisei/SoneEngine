#pragma once
#include <string>
#include <vector>
#include <mutex>
#include <deque>

enum class LogLevel {
    Info,
    Warning,
    Error
};

struct LogEntry {
    LogLevel level;
    std::string message;
    std::string timeStr;
};

class LogManager {
public:
    static LogManager* GetInstance();

    void AddLog(LogLevel level, const std::string& message);
    void ClearLogs();
    
    // ImGui描画用メソッド
    void Draw();

private:
    LogManager() = default;
    ~LogManager() = default;
    LogManager(const LogManager&) = delete;
    LogManager& operator=(const LogManager&) = delete;

    std::deque<LogEntry> logs_;
    std::mutex mutex_;
    
    bool autoScroll_ = true;
};
