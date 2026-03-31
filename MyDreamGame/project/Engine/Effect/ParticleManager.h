#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <string>
#include "Core/Utility/Utilityfunctions.h"
#include "Resource/Model/ModelCommon.h" 
#include "ParticleCommon.h"
#include "Core/Utility/BlendMode.h"
#include <numbers>
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
    Transform transform; // エミッタのTransform（発生位置など）
    uint32_t count;      // 一回で発生する数
    float frequency;     // 発生頻度（秒）
    float frequencyTime; // 頻度用タイマー
};

// CPU側で持つ個々のパーティクル情報
struct ParticleData {
    Transform transform;
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

class ParticleManager {
public:

    ParticleManager() = default;
    virtual~ParticleManager();
    // 初期化
    // count: パーティクルの最大数
    // srvIndex: SRVを作るDescriptorHeapの場所(WindowsApplication.cppで計算していたindex)
    void Initialize(ID3D12GraphicsCommandList *commandList,ParticleCommon *particleCommon, uint32_t count, const std::string &textureFilePath, int srvIndex, BlendMode blendMode = kBlendModeNomal);

    // 更新
    virtual void Update();

    // 描画
    void Draw(const Matrix4x4 &viewProjection);

    // 外部からパーティクルを発生させるための関数
    void Emit(const Emitter &emitter);

    // セッター
    void SetBlendMode(BlendMode blendMode) { blendMode_ = blendMode; }

    // ImGuiを描画する関数
    void DrawImGui();

protected:

    // 座標オフセットを受け取れるように変更
    ParticleData MakeNewParticle(const Vector3 &translate);

    // Emitterの情報からパーティクルリストを生成する関数
    std::list<ParticleData> EmitInternal(const Emitter &emitter);

    // 内部で1つ分のパーティクルデータを生成するヘルパー関数
    ParticleData MakeNewParticle();

    // 当たり判定関数
    bool IsCollision(const AABB &aabb, const Vector3 &point);

    // Updateの最後にこれを呼ばないと描画されない
    void TransferToGPU(const Matrix4x4 &viewProjection);

    // vector から list へ変更
    std::list<ParticleData> particles_{};

    ParticleCommon *particleCommon_ = nullptr;
    uint32_t kParticleCount_ = 0;

    uint32_t numActiveParticles_ = 0;

    // 自分のブレンドモード（デフォルトは通常）
    BlendMode blendMode_ = kBlendModeNomal;

    // Instancing用リソース
    Microsoft::WRL::ComPtr<ID3D12Resource> instancingResource_{};
    D3D12_GPU_DESCRIPTOR_HANDLE instancingSrvHandleGPU_{};

    // マップ先の型を ParticleForGPU に変更
    ParticleForGPU *instancingData_ = nullptr;

    // マテリアル用リソース
    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_{};
    Material *materialData_ = nullptr;

    // テクスチャハンドル
    uint32_t textureIndex_ = 0;

    // 乱数生成器をメンバに持つ
    std::mt19937 randomEngine_;

    // このパーティクルマネージャが持つデフォルトのエミッタ
    Emitter emitter_{};

    // フィールドのメンバ変数
    AccelerationField accelerationField_{};

    // ビルボード有効化フラグ
    bool isBillboard_ = true;
};