#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <string>
#include "Utility/Utilityfunctions.h"
#include "Model/ModelCommon.h" 
#include "ParticleCommon.h"
#include "Utility/BlendMode.h"
#include <numbers>
#include <list>
#include <random>

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

class Particle {
public:

    Particle() = default;
    ~Particle();
    // 初期化
    // count: パーティクルの最大数
    // srvIndex: SRVを作るDescriptorHeapの場所(WindowsApplication.cppで計算していたindex)
    void Initialize(ID3D12GraphicsCommandList *commandList,ParticleCommon *particleCommon, uint32_t count, const std::string &textureFilePath, int srvIndex, BlendMode blendMode = kBlendModeNomal);

    // 更新
    void Update(const Matrix4x4 &viewProjection, const Matrix4x4 &cameraMatrix);

    // 描画
    void Draw();

    // 外部からパーティクルを発生させるための関数
    // count: 一度に出す数
    void Emit(uint32_t count);

    // セッター
    void SetBlendMode(BlendMode blendMode) { blendMode_ = blendMode; }

private:

    // 内部で1つ分のパーティクルデータを生成するヘルパー関数
    ParticleData MakeNewParticle();

    // vector から list へ変更
    std::list<ParticleData> particles_;

    std::vector<ParticleData> particles_;
    ParticleCommon *particleCommon_ = nullptr;
    uint32_t kParticleCount_ = 0;

    uint32_t numActiveParticles_ = 0;

    // 自分のブレンドモード（デフォルトは通常）
    BlendMode blendMode_ = kBlendModeNomal;

    // Instancing用リソース
    Microsoft::WRL::ComPtr<ID3D12Resource> instancingResource_;
    D3D12_GPU_DESCRIPTOR_HANDLE instancingSrvHandleGPU_{};

    // マップ先の型を ParticleForGPU に変更
    ParticleForGPU *instancingData_ = nullptr;

    // マテリアル用リソース
    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
    Material *materialData_ = nullptr;

    // テクスチャハンドル
    uint32_t textureIndex_ = 0;

    // 乱数生成器をメンバに持つ
    std::mt19937 randomEngine_;
};