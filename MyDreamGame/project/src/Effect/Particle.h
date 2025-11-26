#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <string>
#include "Utility/Utilityfunctions.h"
#include "Model/ModelCommon.h" 
#include "ParticleCommon.h"
#include "Utility/BlendMode.h"

class Particle {
public:

    Particle() = default;
    ~Particle();
    // 初期化
    // count: パーティクルの最大数
    // srvIndex: SRVを作るDescriptorHeapの場所(WindowsApplication.cppで計算していたindex)
    void Initialize(ID3D12GraphicsCommandList *commandList,ParticleCommon *particleCommon, uint32_t count, const std::string &textureFilePath, int srvIndex, BlendMode blendMode = kBlendModeNomal);

    // 更新
    void Update(const Matrix4x4 &viewProjection);

    // 描画
    void Draw();

    // セッター
    void SetBlendMode(BlendMode blendMode) { blendMode_ = blendMode; }

private:
    ParticleCommon *particleCommon_ = nullptr;
    uint32_t kParticleCount_ = 0;

    // 自分のブレンドモード（デフォルトは通常）
    BlendMode blendMode_ = kBlendModeNomal;

    // Instancing用リソース
    Microsoft::WRL::ComPtr<ID3D12Resource> instancingResource_;
    TransformMatrix *instancingData_ = nullptr;
    D3D12_GPU_DESCRIPTOR_HANDLE instancingSrvHandleGPU_{};

    // マテリアル用リソース
    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
    Material *materialData_ = nullptr;

    // テクスチャハンドル
    uint32_t textureIndex_ = 0;
};