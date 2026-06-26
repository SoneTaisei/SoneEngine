#include "LogManager.h"
#include "UtilityFunctions.h"
#include <chrono>
#include <format>
#include <ctime>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

LogManager* LogManager::GetInstance() {
    static LogManager instance;
    return &instance;
}

void LogManager::AddLog(LogLevel level, const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    LogEntry entry;
    entry.level = level;
    entry.message = message;
    
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    localtime_s(&tm, &time);
    entry.timeStr = std::format("{:02}:{:02}:{:02}", tm.tm_hour, tm.tm_min, tm.tm_sec);
    
    logs_.push_back(entry);
    
    if (logs_.size() > 1000) {
        logs_.pop_front();
    }
    
    std::string prefix = "";
    switch (level) {
    case LogLevel::Info: prefix = "[Info] "; break;
    case LogLevel::Warning: prefix = "[Warning] "; break;
    case LogLevel::Error: prefix = "[Error] "; break;
    }
    Log(prefix + message + "\n");
}

void LogManager::ClearLogs() {
    std::lock_guard<std::mutex> lock(mutex_);
    logs_.clear();
}

void LogManager::Draw() {
#ifdef USE_IMGUI
    if (!ImGui::Begin("ログ (Log Window)")) {
        ImGui::End();
        return;
    }
    
    if (ImGui::Button("クリア (Clear)")) {
        ClearLogs();
    }
    ImGui::SameLine();
    bool copy = ImGui::Button("コピー (Copy)");
    ImGui::SameLine();
    ImGui::Checkbox("自動スクロール (Auto-scroll)", &autoScroll_);
    
    ImGui::Separator();
    
    ImGui::BeginChild("ScrollingRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
    
    if (copy) {
        ImGui::LogToClipboard();
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& log : logs_) {
        ImVec4 color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // default white
        if (log.level == LogLevel::Warning) {
            color = ImVec4(1.0f, 1.0f, 0.0f, 1.0f); // yellow
        } else if (log.level == LogLevel::Error) {
            color = ImVec4(1.0f, 0.4f, 0.4f, 1.0f); // red
        }
        
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::TextUnformatted(std::format("[{}] {}", log.timeStr, log.message).c_str());
        ImGui::PopStyleColor();
    }
    
    if (copy) {
        ImGui::LogFinish();
    }
    
    if (autoScroll_ && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
        ImGui::SetScrollHereY(1.0f);
    }
    
    ImGui::EndChild();
    ImGui::End();
#endif
}
