#pragma once
#ifdef USE_IMGUI
#include "Core/Utility/Vector3.h"
#include "Core/Utility/Vector4.h"
#include "Core/Utility/TransformFunctions.h"
#include <string>
#include <fstream>
#include <filesystem>
#include <cmath>
#include <numbers>
#include <nlohmann/json.hpp>

// アニメーションエディター専用のスタジオライティング設定
struct AnimationLightingConfig {
    // --- モード切替 ---
    // false: 専用スタジオライティング, true: ゲームシーン本番のライティング設定
    bool useGameLighting = false;

    // --- 直感操作パラメータ ---
    float brightness = 1.0f;           // 全体の明るさスケール (0.2 ~ 3.0)
    float horizontalAngleDeg = 45.0f;  // 光の水平回転角度 (0 ~ 360°)
    float elevationDeg = 45.0f;        // 光の仰角・高さ (10 ~ 80°)
    int currentPresetIndex = 0;        // 現在選択されているプリセット

    // --- 1. キーライト (Directional Light / 平行光源) ---
    bool enableKeyLight = true;
    Vector4 keyLightColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    Vector3 keyLightDirection = { 0.45f, -0.75f, 0.48f };
    float keyLightIntensity = 1.2f;

    // --- 2. リムライト / フィルライト (Point Light / 点光源) ---
    bool enableRimLight = true;
    Vector4 rimLightColor = { 0.85f, 0.92f, 1.0f, 1.0f };
    Vector3 rimLightPos = { -2.0f, 3.5f, -3.0f };
    float rimLightIntensity = 1.0f;
    float rimLightRadius = 15.0f;
    float rimLightDecay = 1.0f;

    // --- 3. 環境光 (Ambient Intensity) ---
    float ambientIntensity = 0.55f;

    // 水平角・仰角から方向ベクトルを自動再計算
    void RecalculateDirection() {
        constexpr float kPi = static_cast<float>(std::numbers::pi);
        float radH = horizontalAngleDeg * kPi / 180.0f;
        float radE = elevationDeg * kPi / 180.0f;
        float cosE = std::cos(radE);
        float sinE = std::sin(radE);

        // 球面座標から光の方向（光源からモデルへ向かうベクトル）を算出
        // 光源位置: (cosE * sinH, sinE, cosE * cosH) -> モデル(0,0,0)へ向かう向きは反転
        Vector3 dir = {
            -cosE * std::sin(radH),
            -sinE,
            -cosE * std::cos(radH)
        };
        keyLightDirection = TransformFunctions::Normalize(dir);
    }

    // プリセット適用
    void ApplyPreset(int presetId) {
        currentPresetIndex = presetId;
        switch (presetId) {
        case 0: // [スタジオ標準] (自然な陰影と輪郭リムライト)
            enableKeyLight = true;
            keyLightColor = { 1.0f, 1.0f, 1.0f, 1.0f };
            horizontalAngleDeg = 45.0f;
            elevationDeg = 45.0f;
            keyLightIntensity = 1.2f;
            enableRimLight = true;
            rimLightColor = { 0.85f, 0.92f, 1.0f, 1.0f };
            rimLightPos = { -2.0f, 3.5f, -3.0f };
            rimLightIntensity = 1.0f;
            rimLightRadius = 15.0f;
            ambientIntensity = 0.55f;
            break;

        case 1: // [明るい全体光] (全体が均一に明るく細部が見やすい)
            enableKeyLight = true;
            keyLightColor = { 1.0f, 1.0f, 1.0f, 1.0f };
            horizontalAngleDeg = 30.0f;
            elevationDeg = 50.0f;
            keyLightIntensity = 1.5f;
            enableRimLight = true;
            rimLightColor = { 0.9f, 0.95f, 1.0f, 1.0f };
            rimLightPos = { 0.0f, 4.0f, -2.5f };
            rimLightIntensity = 1.2f;
            rimLightRadius = 20.0f;
            ambientIntensity = 0.85f;
            break;

        case 2: // [クッキリ陰影] (コントラスト強めでポーズ・凹凸強調)
            enableKeyLight = true;
            keyLightColor = { 1.0f, 1.0f, 1.0f, 1.0f };
            horizontalAngleDeg = 75.0f;
            elevationDeg = 30.0f;
            keyLightIntensity = 1.6f;
            enableRimLight = true;
            rimLightColor = { 0.8f, 0.88f, 1.0f, 1.0f };
            rimLightPos = { -3.0f, 2.5f, -3.0f };
            rimLightIntensity = 0.6f;
            rimLightRadius = 12.0f;
            ambientIntensity = 0.35f;
            break;

        case 3: // [正面ライト] (正面から照らし影を最小限にする)
            enableKeyLight = true;
            keyLightColor = { 1.0f, 1.0f, 1.0f, 1.0f };
            horizontalAngleDeg = 0.0f;
            elevationDeg = 15.0f;
            keyLightIntensity = 1.3f;
            enableRimLight = false;
            ambientIntensity = 0.65f;
            break;

        case 4: // [輪郭シルエット] (暗めの背景で輪郭エッジを強調)
            enableKeyLight = true;
            keyLightColor = { 0.7f, 0.7f, 0.75f, 1.0f };
            horizontalAngleDeg = 180.0f;
            elevationDeg = 30.0f;
            keyLightIntensity = 0.3f;
            enableRimLight = true;
            rimLightColor = { 0.6f, 0.85f, 1.0f, 1.0f };
            rimLightPos = { 0.0f, 3.5f, -4.0f };
            rimLightIntensity = 2.2f;
            rimLightRadius = 18.0f;
            ambientIntensity = 0.20f;
            break;
        }
        RecalculateDirection();
    }

    void ResetToDefault() {
        useGameLighting = false;
        brightness = 1.0f;
        ApplyPreset(0);
    }

    // ローカル設定パス
    static const char* GetLocalFilePath() {
        return "resources/json/local/animation_lighting.json";
    }

    // JSONから読み込み（旧sharedファイルからの移行もサポート）
    bool LoadFromFile(const std::string& filePath = "") {
        std::string targetPath = filePath.empty() ? GetLocalFilePath() : filePath;
        
        // ローカル設定がない場合、旧sharedパスがあれば読み込んで移行
        if (!std::filesystem::exists(targetPath)) {
            std::string oldSharedPath = "resources/json/shared/Animation/animation_lighting.json";
            if (std::filesystem::exists(oldSharedPath)) {
                targetPath = oldSharedPath;
            } else {
                ResetToDefault();
                return false;
            }
        }

        std::ifstream ifs(targetPath);
        if (!ifs.is_open()) return false;

        try {
            nlohmann::json j;
            ifs >> j;

            if (j.contains("useGameLighting")) useGameLighting = j["useGameLighting"];
            if (j.contains("brightness")) brightness = j["brightness"];
            if (j.contains("horizontalAngleDeg")) horizontalAngleDeg = j["horizontalAngleDeg"];
            if (j.contains("elevationDeg")) elevationDeg = j["elevationDeg"];
            if (j.contains("currentPresetIndex")) currentPresetIndex = j["currentPresetIndex"];

            if (j.contains("enableKeyLight")) enableKeyLight = j["enableKeyLight"];
            if (j.contains("keyLightIntensity")) keyLightIntensity = j["keyLightIntensity"];
            if (j.contains("keyLightColor") && j["keyLightColor"].is_array() && j["keyLightColor"].size() >= 4) {
                keyLightColor = { j["keyLightColor"][0], j["keyLightColor"][1], j["keyLightColor"][2], j["keyLightColor"][3] };
            }
            if (j.contains("keyLightDirection") && j["keyLightDirection"].is_array() && j["keyLightDirection"].size() >= 3) {
                keyLightDirection = { j["keyLightDirection"][0], j["keyLightDirection"][1], j["keyLightDirection"][2] };
            } else {
                RecalculateDirection();
            }

            if (j.contains("enableRimLight")) enableRimLight = j["enableRimLight"];
            if (j.contains("rimLightIntensity")) rimLightIntensity = j["rimLightIntensity"];
            if (j.contains("rimLightRadius")) rimLightRadius = j["rimLightRadius"];
            if (j.contains("rimLightDecay")) rimLightDecay = j["rimLightDecay"];
            if (j.contains("rimLightColor") && j["rimLightColor"].is_array() && j["rimLightColor"].size() >= 4) {
                rimLightColor = { j["rimLightColor"][0], j["rimLightColor"][1], j["rimLightColor"][2], j["rimLightColor"][3] };
            }
            if (j.contains("rimLightPos") && j["rimLightPos"].is_array() && j["rimLightPos"].size() >= 3) {
                rimLightPos = { j["rimLightPos"][0], j["rimLightPos"][1], j["rimLightPos"][2] };
            }

            if (j.contains("ambientIntensity")) ambientIntensity = j["ambientIntensity"];

            // 旧ファイルからのロードだった場合、ローカルに保存し直す
            if (targetPath != GetLocalFilePath()) {
                SaveToFile();
                try { std::filesystem::remove(targetPath); } catch (...) {}
            }
            return true;
        } catch (...) {
            return false;
        }
    }

    // JSONへ保存 (ローカルフォルダへ保存)
    bool SaveToFile(const std::string& filePath = "") const {
        std::string targetPath = filePath.empty() ? GetLocalFilePath() : filePath;
        try {
            std::filesystem::path p(targetPath);
            if (p.has_parent_path() && !std::filesystem::exists(p.parent_path())) {
                std::filesystem::create_directories(p.parent_path());
            }

            nlohmann::json j;
            j["useGameLighting"] = useGameLighting;
            j["brightness"] = brightness;
            j["horizontalAngleDeg"] = horizontalAngleDeg;
            j["elevationDeg"] = elevationDeg;
            j["currentPresetIndex"] = currentPresetIndex;

            j["enableKeyLight"] = enableKeyLight;
            j["keyLightIntensity"] = keyLightIntensity;
            j["keyLightColor"] = { keyLightColor.x, keyLightColor.y, keyLightColor.z, keyLightColor.w };
            j["keyLightDirection"] = { keyLightDirection.x, keyLightDirection.y, keyLightDirection.z };

            j["enableRimLight"] = enableRimLight;
            j["rimLightIntensity"] = rimLightIntensity;
            j["rimLightRadius"] = rimLightRadius;
            j["rimLightDecay"] = rimLightDecay;
            j["rimLightColor"] = { rimLightColor.x, rimLightColor.y, rimLightColor.z, rimLightColor.w };
            j["rimLightPos"] = { rimLightPos.x, rimLightPos.y, rimLightPos.z };

            j["ambientIntensity"] = ambientIntensity;

            std::ofstream ofs(targetPath);
            if (!ofs.is_open()) return false;
            ofs << j.dump(4);
            return true;
        } catch (...) {
            return false;
        }
    }
};
#endif
