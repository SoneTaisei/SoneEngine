#pragma once
#include <unordered_set>
#include <vector>
#include <string>
#include <fstream>
#include <filesystem>
#include <nlohmann/json.hpp>

/// <summary>
/// ステージクリア状況の永続化・判定ヘルパー
/// resources/json/local/stage_clear.json に保存・読み込みを行う
/// </summary>
class StageClearData {
public:
    static inline const char* kSavePath = "resources/json/local/stage_clear.json";

    /// <summary>指定ステージがクリア済みか判定 (0: チュートリアル, 1: ステージ1, 2: ステージ2, 3: ステージ3)</summary>
    static bool IsCleared(int stageIndex) {
        EnsureLoaded();
        return s_ClearedStages.contains(stageIndex);
    }

    /// <summary>指定ステージのクリアフラグを設定</summary>
    static void SetCleared(int stageIndex, bool cleared = true) {
        EnsureLoaded();
        if (cleared) {
            s_ClearedStages.insert(stageIndex);
        } else {
            s_ClearedStages.erase(stageIndex);
        }
        Save();
    }

    /// <summary>セーブファイルに書き込み</summary>
    static void Save() {
        EnsureLoaded();
        try {
            std::filesystem::path path(kSavePath);
            if (path.has_parent_path()) {
                std::filesystem::create_directories(path.parent_path());
            }
            nlohmann::json j;
            j["clearedStages"] = std::vector<int>(s_ClearedStages.begin(), s_ClearedStages.end());
            std::ofstream ofs(kSavePath);
            if (ofs.is_open()) {
                ofs << j.dump(4);
            }
        } catch (...) {}
    }

    /// <summary>セーブファイルから読み込み</summary>
    static void Load() {
        s_ClearedStages.clear();
        s_IsLoaded = true;
        std::ifstream ifs(kSavePath);
        if (!ifs.is_open()) return;
        try {
            nlohmann::json j;
            ifs >> j;
            if (j.contains("clearedStages") && j["clearedStages"].is_array()) {
                for (const auto& item : j["clearedStages"]) {
                    if (item.is_number_integer()) {
                        s_ClearedStages.insert(item.get<int>());
                    }
                }
            }
        } catch (...) {}
    }

    /// <summary>全ステージのクリア状況をリセット</summary>
    static void ResetAll() {
        s_ClearedStages.clear();
        Save();
    }

    static const std::unordered_set<int>& GetClearedStages() {
        EnsureLoaded();
        return s_ClearedStages;
    }

private:
    static void EnsureLoaded() {
        if (!s_IsLoaded) {
            Load();
        }
    }

    static inline bool s_IsLoaded = false;
    static inline std::unordered_set<int> s_ClearedStages;
};
