#include "ParameterManager.h"
#include <fstream>
#include <filesystem>
#include <iostream>
#ifdef USE_IMGUI
#include "imgui.h"
#endif

void ParameterManager::Load(const std::string& filepath) {
    filepath_ = filepath;
    if (!std::filesystem::exists(filepath_)) return;

    std::ifstream file(filepath_);
    if (file.is_open()) {
        file >> data_;
        file.close();
    }
}

void ParameterManager::Save() {
    std::filesystem::path path(filepath_);
    if (path.has_parent_path() && !std::filesystem::exists(path.parent_path())) {
        std::filesystem::create_directories(path.parent_path());
    }

    std::ofstream file(filepath_);
    if (file.is_open()) {
        file << data_.dump(4);
        file.close();
    }
}

void ParameterManager::DisplayImGui() {
#ifdef USE_IMGUI
    if (ImGui::Button("Save Parameters")) {
        Save();
    }
    ImGui::SameLine();
    if (ImGui::Button("Load Parameters")) {
        Load(filepath_);
    }

    for (auto& [group, items] : data_.items()) {
        if (ImGui::TreeNode(group.c_str())) {
            for (auto& [key, val] : items.items()) {
                if (val.is_number_float()) {
                    float v = val.get<float>();
                    if (ImGui::DragFloat(key.c_str(), &v, 0.1f)) {
                        val = v;
                    }
                } else if (val.is_number_integer()) {
                    int v = val.get<int>();
                    if (ImGui::DragInt(key.c_str(), &v)) {
                        val = v;
                    }
                } else if (val.is_boolean()) {
                    bool v = val.get<bool>();
                    if (ImGui::Checkbox(key.c_str(), &v)) {
                        val = v;
                    }
                } else if (val.is_string()) {
                    std::string v = val.get<std::string>();
                    char buf[256];
                    strncpy_s(buf, sizeof(buf), v.c_str(), _TRUNCATE);
                    if (ImGui::InputText(key.c_str(), buf, sizeof(buf))) {
                        val = std::string(buf);
                    }
                }
            }
            ImGui::TreePop();
        }
    }
#endif
}
