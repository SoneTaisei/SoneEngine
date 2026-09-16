#include "ParameterManager.h"
#include <fstream>
#include <filesystem>
#include <iostream>
#ifdef USE_IMGUI
#include "imgui.h"
#endif

void ParameterManager::Load(const std::string& filepath) {
    if (!filepath.empty()) {
        filepath_ = filepath;
    }

    // 読み込み候補パス（実行先ディレクトリとプロジェクトディレクトリの両方をチェック）
    std::vector<std::string> searchPaths = {
        filepath_,
        "resources/json/shared/Global/parameters.json",
        "../../project/resources/json/shared/Global/parameters.json",
        "../project/resources/json/shared/Global/parameters.json"
    };

    for (const auto& path : searchPaths) {
        if (std::filesystem::exists(path)) {
            std::ifstream file(path);
            if (file.is_open()) {
                try {
                    nlohmann::json loaded;
                    file >> loaded;
                    file.close();
                    if (!loaded.is_null()) {
                        // 既存のキーを保持しつつ上書き
                        for (auto& [gKey, gVal] : loaded.items()) {
                            for (auto& [kKey, kVal] : gVal.items()) {
                                data_[gKey][kKey] = kVal;
                            }
                        }
                    }
                    filepath_ = path;
                    break;
                } catch (...) {
                    // JSONパースエラー時は次のパスを試す
                }
            }
        }
    }
}

void ParameterManager::Save() {
    // 1. 設定されているパスへ保存
    std::vector<std::string> savePaths = {
        filepath_,
        "resources/json/shared/Global/parameters.json",
        "../../project/resources/json/shared/Global/parameters.json"
    };

    for (const auto& targetPath : savePaths) {
        try {
            std::filesystem::path p(targetPath);
            if (p.has_parent_path() && !std::filesystem::exists(p.parent_path())) {
                std::filesystem::create_directories(p.parent_path());
            }
            std::ofstream file(targetPath);
            if (file.is_open()) {
                file << data_.dump(4);
                file.close();
            }
        } catch (...) {
            // パスが存在しない場合は無視
        }
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
