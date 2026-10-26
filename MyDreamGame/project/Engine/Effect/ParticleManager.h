#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <string>
#include "Core/Utility/Utilityfunctions.h"
#include "ParticleCommon.h"
#include "Core/Utility/BlendMode.h"
#include <list>
#include <random>

struct AABB {
    Vector3 min;
    Vector3 max;
};

struct AccelerationField {
    Vector3 acceleration; // 加速度
    AABB area;            // 範囲
};

struct Emitter {
    EulerTransform transform; // エミッタのTransform（発生位置など）
    uint32_t count;      // 一回で発生する数
    float frequency;     // 発生頻度（秒）
    float frequencyTime; // 頻度用タイマー
};

// CPU側で持つ個々のパーティクル情報
struct ParticleData {
    EulerTransform transform;
    Vector3 velocity; // 速度
    Vector4 color;    // 色
    float lifeTime;
    float currentTime;
};

// GPUに送るための専用構造体 (資料スライド2枚目)
struct ParticleForGPU {
    Matrix4x4 WVP;
    Matrix4x4 World;
    Vector4 color; // 色
};

struct ParticleProperty {
    Vector3 scale = { 1.0f, 1.0f, 1.0f };
    Vector3 rotate = { 0.0f, 0.0f, 0.0f };
    Vector3 velocity = { 0.0f, 0.0f, 0.0f };
    Vector4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
    float lifeTime = 1.0f;
};

class Renderer;

class ParticleManager {
public:
    ParticleManager() = default;
    virtual~ParticleManager();
    // 初期化
    // count: パーティクルの最大数
    // srvIndex: SRVを作るDescriptorHeapの場所(WindowsApplication.cppで計算していたindex)
    void Initialize(ID3D12GraphicsCommandList *commandList,ParticleCommon *particleCommon, uint32_t count, const std::string &textureFilePath, int srvIndex, BlendMode blendMode = kBlendModeNormal);

    // 更新
    virtual void Update();

    // 描画
    void Draw(const Matrix4x4 &viewProjection);

    // 外部からパーティクルを発生させるための関数
    void Emit(const Emitter &emitter);

    // 詳細なパラメータを指定して発生させる
    void EmitCustom(const Vector3& position, const ParticleProperty& minProp, const ParticleProperty& maxProp, uint32_t count);

    // セッター
    void SetBlendMode(BlendMode blendMode) { blendMode_ = blendMode; }

    // 名前
    const std::string &GetName() const { return name_; }
    void SetName(const std::string &name) { name_ = name; }

    // ImGuiを描画する関数
    virtual void DrawImGui();

    // --- 描画・Renderer用公開インターフェース ---
    void TransferToGPU(const Matrix4x4 &viewProjection);
    ParticleCommon *GetParticleCommon() const { return particleCommon_; }
    ID3D12Resource *GetMaterialResource() const { return materialResource_.Get(); }
    D3D12_GPU_DESCRIPTOR_HANDLE GetInstancingSrvHandleGPU() const { return instancingSrvHandleGPU_; }
    uint32_t GetTextureIndex() const { return textureIndex_; }
    uint32_t GetNumActiveParticles() const { return numActiveParticles_; }

protected:
    // 派生クラス用ヘルパー関数
    ParticleData MakeNewParticle(const Vector3 &translate);
    std::list<ParticleData> EmitInternal(const Emitter &emitter);
    ParticleData MakeNewParticle();
    bool IsCollision(const AABB &aabb, const Vector3 &point);
    // 派生クラス用アクセサ
    Emitter &GetEmitter() { return emitter_; }
    const Emitter &GetEmitter() const { return emitter_; }
    std::list<ParticleData> &GetParticles() { return particles_; }
    const std::list<ParticleData> &GetParticles() const { return particles_; }
    std::mt19937 &GetRandomEngine() { return randomEngine_; }
    const AccelerationField &GetAccelerationField() const { return accelerationField_; }

private:
    std::string name_ = "ParticleSystem";
    // パーティクルリスト
    std::list<ParticleData> particles_{};
    ParticleCommon *particleCommon_ = nullptr;
    uint32_t kParticleCount_ = 0;
    uint32_t numActiveParticles_ = 0;
    // 自分のブレンドモード（デフォルトは通常）
    BlendMode blendMode_ = kBlendModeNormal;
    // Instancing用リソース
    Microsoft::WRL::ComPtr<ID3D12Resource> instancingResource_{};
    D3D12_GPU_DESCRIPTOR_HANDLE instancingSrvHandleGPU_{};
    ParticleForGPU *instancingData_ = nullptr;
    // マテリアル用リソース
    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_{};
    Material *materialData_ = nullptr;
    // テクスチャハンドル
    uint32_t textureIndex_ = 0;
    AccelerationField accelerationField_;
    Emitter emitter_;
    std::mt19937 randomEngine_;
    bool isBillboard_ = true;
};