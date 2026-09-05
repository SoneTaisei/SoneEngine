#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <fstream>
#include <nlohmann/json.hpp>
#include "Core/Utility/UtilityFunctions.h"
#include "Core/Utility/BlendMode.h"

// パーティクルの描画形状タイプ
enum class GPUParticleRenderType {
    Sprite = 0, // 板ポリゴン（スプライト）
    Mesh   = 1  // 3Dモデルメッシュ
};

// ビルボード化の方式
enum class GPUParticleBillboardType {
    None            = 0, // ビルボードなし（3D回転に従う、瓦礫や破片用）
    AllAxis         = 1, // 全方向カメラ追従（標準的な光や煙用）
    YAxis           = 2, // Y軸固定ビルボード（火柱や草木用）
    VelocityStretch = 3  // 進行方向ストレッチ（火花、雨、レーザー用）
};

// 発生形状
enum class GPUParticleSpawnShape {
    Point  = 0, // 点
    Sphere = 1, // 球
    Box    = 2, // 直方体
    Cone   = 3, // 円錐
    Ring   = 4  // リング
};

// バースト設定
struct GPUParticleBurst {
    float time = 0.0f;       // 発火タイミング（秒）
    uint32_t count = 10;     // 発生個数
};

// カラーキー（Lifetimeグラデーション用）
struct GPUParticleColorKey {
    float time = 0.0f;       // 0.0f ~ 1.0f
    Vector4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
};

// サイズキー（Lifetimeサイズ変化用）
struct GPUParticleSizeKey {
    float time = 0.0f;       // 0.0f ~ 1.0f
    float size = 1.0f;
};

// 単一エミッターの設定データ
struct GPUParticleEmitterData {
    std::string name = "New Emitter";
    bool enabled = true;
    bool solo = false;
    bool mute = false;

    // レンダラー設定
    GPUParticleRenderType renderType = GPUParticleRenderType::Sprite;
    std::string modelPath = "resources/Object/School/sphere/sphere.obj";
    std::string texturePath = "resources/Sprite/School/circle.png";
    GPUParticleBillboardType billboardType = GPUParticleBillboardType::AllAxis;
    BlendMode blendMode = BlendMode::kBlendModeAdd;

    // 基本設定
    uint32_t maxParticles = 500;
    float duration = 2.0f;          // エミッター稼働時間（秒）
    float startDelay = 0.0f;        // 発火遅延時間（秒）
    bool isLoop = true;             // ループ再生

    // 発生レート & バースト
    float spawnRate = 50.0f;        // 1秒あたりの発生数
    std::vector<GPUParticleBurst> bursts;

    // 発生形状
    GPUParticleSpawnShape shape = GPUParticleSpawnShape::Point;
    float shapeRadius = 1.0f;
    Vector3 shapeBoxSize = { 1.0f, 1.0f, 1.0f };
    float shapeConeAngle = 30.0f;   // 度数法
    float shapeConeRadius = 0.5f;

    // 寿命
    float lifetimeMin = 1.0f;
    float lifetimeMax = 2.0f;

    // 速度・運動
    float initialSpeedMin = 1.0f;
    float initialSpeedMax = 3.0f;
    Vector3 initialVelocityDir = { 0.0f, 1.0f, 0.0f }; // 主方向
    float velocitySpread = 45.0f;   // 散開角度（度数法）
    float gravity = -9.8f;          // 重力加速度 (Y軸)
    float drag = 0.0f;             // 空気抵抗減速

    // 回転
    Vector3 initialRotateMin = { 0.0f, 0.0f, 0.0f };
    Vector3 initialRotateMax = { 0.0f, 0.0f, 0.0f };
    Vector3 rotateSpeedMin = { 0.0f, 0.0f, 0.0f };
    Vector3 rotateSpeedMax = { 0.0f, 0.0f, 0.0f };

    // サイズ
    Vector3 initialScaleMin = { 0.5f, 0.5f, 0.5f };
    Vector3 initialScaleMax = { 0.5f, 0.5f, 0.5f };
    float endScaleFactor = 0.0f;    // 寿命終了時のサイズ倍率（0で消滅）
    float stretchFactor = 0.1f;     // VelocityStretch時の長さスケール

    // カラー & アルファ
    Vector4 startColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    Vector4 endColor = { 1.0f, 1.0f, 1.0f, 0.0f };
    bool useColorGradient = false;
    std::vector<GPUParticleColorKey> colorKeys;
};

// 複合パーティクルシステム全体の設定データ（複数のエミッターを束ねる）
struct GPUParticleSystemData {
    std::string systemName = "CompositeParticle";
    float duration = 3.0f;          // システム全体の再生時間（秒）
    bool isLoop = true;             // システム全体のループ
    std::vector<GPUParticleEmitterData> emitters;
};

// ==========================================
// JSON シリアライズ / デシリアライズ実装
// ==========================================

inline void to_json(nlohmann::json& j, const GPUParticleBurst& b) {
    j = nlohmann::json{ {"time", b.time}, {"count", b.count} };
}

inline void from_json(const nlohmann::json& j, GPUParticleBurst& b) {
    if (j.contains("time")) b.time = j.at("time").get<float>();
    if (j.contains("count")) b.count = j.at("count").get<uint32_t>();
}

inline void to_json(nlohmann::json& j, const GPUParticleColorKey& k) {
    j = nlohmann::json{
        {"time", k.time},
        {"color", {k.color.x, k.color.y, k.color.z, k.color.w}}
    };
}

inline void from_json(const nlohmann::json& j, GPUParticleColorKey& k) {
    if (j.contains("time")) k.time = j.at("time").get<float>();
    if (j.contains("color") && j["color"].is_array() && j["color"].size() >= 4) {
        k.color = { j["color"][0], j["color"][1], j["color"][2], j["color"][3] };
    }
}

inline void to_json(nlohmann::json& j, const GPUParticleEmitterData& e) {
    j = nlohmann::json{
        {"name", e.name},
        {"enabled", e.enabled},
        {"renderType", static_cast<int>(e.renderType)},
        {"modelPath", e.modelPath},
        {"texturePath", e.texturePath},
        {"billboardType", static_cast<int>(e.billboardType)},
        {"blendMode", static_cast<int>(e.blendMode)},
        {"maxParticles", e.maxParticles},
        {"duration", e.duration},
        {"startDelay", e.startDelay},
        {"isLoop", e.isLoop},
        {"spawnRate", e.spawnRate},
        {"bursts", e.bursts},
        {"shape", static_cast<int>(e.shape)},
        {"shapeRadius", e.shapeRadius},
        {"shapeBoxSize", {e.shapeBoxSize.x, e.shapeBoxSize.y, e.shapeBoxSize.z}},
        {"shapeConeAngle", e.shapeConeAngle},
        {"shapeConeRadius", e.shapeConeRadius},
        {"lifetimeMin", e.lifetimeMin},
        {"lifetimeMax", e.lifetimeMax},
        {"initialSpeedMin", e.initialSpeedMin},
        {"initialSpeedMax", e.initialSpeedMax},
        {"initialVelocityDir", {e.initialVelocityDir.x, e.initialVelocityDir.y, e.initialVelocityDir.z}},
        {"velocitySpread", e.velocitySpread},
        {"gravity", e.gravity},
        {"drag", e.drag},
        {"initialRotateMin", {e.initialRotateMin.x, e.initialRotateMin.y, e.initialRotateMin.z}},
        {"initialRotateMax", {e.initialRotateMax.x, e.initialRotateMax.y, e.initialRotateMax.z}},
        {"rotateSpeedMin", {e.rotateSpeedMin.x, e.rotateSpeedMin.y, e.rotateSpeedMin.z}},
        {"rotateSpeedMax", {e.rotateSpeedMax.x, e.rotateSpeedMax.y, e.rotateSpeedMax.z}},
        {"initialScaleMin", {e.initialScaleMin.x, e.initialScaleMin.y, e.initialScaleMin.z}},
        {"initialScaleMax", {e.initialScaleMax.x, e.initialScaleMax.y, e.initialScaleMax.z}},
        {"endScaleFactor", e.endScaleFactor},
        {"stretchFactor", e.stretchFactor},
        {"startColor", {e.startColor.x, e.startColor.y, e.startColor.z, e.startColor.w}},
        {"endColor", {e.endColor.x, e.endColor.y, e.endColor.z, e.endColor.w}},
        {"useColorGradient", e.useColorGradient},
        {"colorKeys", e.colorKeys}
    };
}

inline void from_json(const nlohmann::json& j, GPUParticleEmitterData& e) {
    if (j.contains("name")) e.name = j.at("name").get<std::string>();
    if (j.contains("enabled")) e.enabled = j.at("enabled").get<bool>();
    if (j.contains("renderType")) e.renderType = static_cast<GPUParticleRenderType>(j.at("renderType").get<int>());
    if (j.contains("modelPath")) e.modelPath = j.at("modelPath").get<std::string>();
    if (j.contains("texturePath")) e.texturePath = j.at("texturePath").get<std::string>();
    if (j.contains("billboardType")) e.billboardType = static_cast<GPUParticleBillboardType>(j.at("billboardType").get<int>());
    if (j.contains("blendMode")) e.blendMode = static_cast<BlendMode>(j.at("blendMode").get<int>());
    if (j.contains("maxParticles")) e.maxParticles = j.at("maxParticles").get<uint32_t>();
    if (j.contains("duration")) e.duration = j.at("duration").get<float>();
    if (j.contains("startDelay")) e.startDelay = j.at("startDelay").get<float>();
    if (j.contains("isLoop")) e.isLoop = j.at("isLoop").get<bool>();
    if (j.contains("spawnRate")) e.spawnRate = j.at("spawnRate").get<float>();
    if (j.contains("bursts")) e.bursts = j.at("bursts").get<std::vector<GPUParticleBurst>>();
    if (j.contains("shape")) e.shape = static_cast<GPUParticleSpawnShape>(j.at("shape").get<int>());
    if (j.contains("shapeRadius")) e.shapeRadius = j.at("shapeRadius").get<float>();
    if (j.contains("shapeBoxSize") && j["shapeBoxSize"].size() >= 3) {
        e.shapeBoxSize = { j["shapeBoxSize"][0], j["shapeBoxSize"][1], j["shapeBoxSize"][2] };
    }
    if (j.contains("shapeConeAngle")) e.shapeConeAngle = j.at("shapeConeAngle").get<float>();
    if (j.contains("shapeConeRadius")) e.shapeConeRadius = j.at("shapeConeRadius").get<float>();
    if (j.contains("lifetimeMin")) e.lifetimeMin = j.at("lifetimeMin").get<float>();
    if (j.contains("lifetimeMax")) e.lifetimeMax = j.at("lifetimeMax").get<float>();
    if (j.contains("initialSpeedMin")) e.initialSpeedMin = j.at("initialSpeedMin").get<float>();
    if (j.contains("initialSpeedMax")) e.initialSpeedMax = j.at("initialSpeedMax").get<float>();
    if (j.contains("initialVelocityDir") && j["initialVelocityDir"].size() >= 3) {
        e.initialVelocityDir = { j["initialVelocityDir"][0], j["initialVelocityDir"][1], j["initialVelocityDir"][2] };
    }
    if (j.contains("velocitySpread")) e.velocitySpread = j.at("velocitySpread").get<float>();
    if (j.contains("gravity")) e.gravity = j.at("gravity").get<float>();
    if (j.contains("drag")) e.drag = j.at("drag").get<float>();
    if (j.contains("initialRotateMin") && j["initialRotateMin"].size() >= 3) {
        e.initialRotateMin = { j["initialRotateMin"][0], j["initialRotateMin"][1], j["initialRotateMin"][2] };
    }
    if (j.contains("initialRotateMax") && j["initialRotateMax"].size() >= 3) {
        e.initialRotateMax = { j["initialRotateMax"][0], j["initialRotateMax"][1], j["initialRotateMax"][2] };
    }
    if (j.contains("rotateSpeedMin") && j["rotateSpeedMin"].size() >= 3) {
        e.rotateSpeedMin = { j["rotateSpeedMin"][0], j["rotateSpeedMin"][1], j["rotateSpeedMin"][2] };
    }
    if (j.contains("rotateSpeedMax") && j["rotateSpeedMax"].size() >= 3) {
        e.rotateSpeedMax = { j["rotateSpeedMax"][0], j["rotateSpeedMax"][1], j["rotateSpeedMax"][2] };
    }
    if (j.contains("initialScaleMin") && j["initialScaleMin"].size() >= 3) {
        e.initialScaleMin = { j["initialScaleMin"][0], j["initialScaleMin"][1], j["initialScaleMin"][2] };
    }
    if (j.contains("initialScaleMax") && j["initialScaleMax"].size() >= 3) {
        e.initialScaleMax = { j["initialScaleMax"][0], j["initialScaleMax"][1], j["initialScaleMax"][2] };
    }
    if (j.contains("endScaleFactor")) e.endScaleFactor = j.at("endScaleFactor").get<float>();
    if (j.contains("stretchFactor")) e.stretchFactor = j.at("stretchFactor").get<float>();
    if (j.contains("startColor") && j["startColor"].size() >= 4) {
        e.startColor = { j["startColor"][0], j["startColor"][1], j["startColor"][2], j["startColor"][3] };
    }
    if (j.contains("endColor") && j["endColor"].size() >= 4) {
        e.endColor = { j["endColor"][0], j["endColor"][1], j["endColor"][2], j["endColor"][3] };
    }
    if (j.contains("useColorGradient")) e.useColorGradient = j.at("useColorGradient").get<bool>();
    if (j.contains("colorKeys")) e.colorKeys = j.at("colorKeys").get<std::vector<GPUParticleColorKey>>();
}

inline void to_json(nlohmann::json& j, const GPUParticleSystemData& s) {
    j = nlohmann::json{
        {"systemName", s.systemName},
        {"duration", s.duration},
        {"isLoop", s.isLoop},
        {"emitters", s.emitters}
    };
}

inline void from_json(const nlohmann::json& j, GPUParticleSystemData& s) {
    if (j.contains("systemName")) s.systemName = j.at("systemName").get<std::string>();
    if (j.contains("duration")) s.duration = j.at("duration").get<float>();
    if (j.contains("isLoop")) s.isLoop = j.at("isLoop").get<bool>();
    if (j.contains("emitters")) s.emitters = j.at("emitters").get<std::vector<GPUParticleEmitterData>>();
}

inline bool SaveParticleSystemToJson(const GPUParticleSystemData& data, const std::string& filePath) {
    try {
        nlohmann::json j = data;
        std::ofstream file(filePath);
        if (!file.is_open()) return false;
        file << j.dump(4);
        return true;
    } catch (...) {
        return false;
    }
}

inline bool LoadParticleSystemFromJson(GPUParticleSystemData& outData, const std::string& filePath) {
    try {
        std::ifstream file(filePath);
        if (!file.is_open()) return false;
        nlohmann::json j;
        file >> j;
        outData = j.get<GPUParticleSystemData>();
        return true;
    } catch (...) {
        return false;
    }
}
