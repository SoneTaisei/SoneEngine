#pragma once
#include <string>
#include <nlohmann/json.hpp>

class ParameterManager {
public:
    static ParameterManager* GetInstance() {
        static ParameterManager instance;
        return &instance;
    }

    void Load(const std::string& filepath);
    void Save();

    template<typename T>
    T GetValue(const std::string& group, const std::string& key, const T& defaultValue) {
        if (data_.contains(group) && data_[group].contains(key)) {
            return data_[group][key].get<T>();
        }
        // If not exists, insert the default value
        data_[group][key] = defaultValue;
        return defaultValue;
    }

    template<typename T>
    void SetValue(const std::string& group, const std::string& key, const T& value) {
        data_[group][key] = value;
    }

    // Renders ImGui controls for all parameters
    void DisplayImGui();

    void SetFilePath(const std::string& filepath) { filepath_ = filepath; }

private:
    ParameterManager() = default;
    ~ParameterManager() = default;

    nlohmann::json data_;
    std::string filepath_ = "resources/json/Global/parameters.json";
};
