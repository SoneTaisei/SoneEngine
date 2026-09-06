#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <vector>
#include <random>
#include "GPUParticleData.h"
#include "Effect/ParticleCommon.h"
#include "Effect/ParticleManager.h"
#include "Resource/Model/Model.h"
#include "Resource/Model/ModelManager.h"

// CPU側でシミュレーションする1パーティクルの状態
struct GPUParticleInstance {
    Vector3 position = { 0.0f, 0.0f, 0.0f };
    Vector3 velocity = { 0.0f, 0.0f, 0.0f };
    Vector3 initialScale = { 1.0f, 1.0f, 1.0f };
    Vector3 scale = { 1.0f, 1.0f, 1.0f };
    Vector3 rotate = { 0.0f, 0.0f, 0.0f };
    Vector3 rotateSpeed = { 0.0f, 0.0f, 0.0f };
    Vector4 startColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    Vector4 endColor = { 1.0f, 1.0f, 1.0f, 0.0f };
    Vector4 currentColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    float lifeTime = 1.0f;
    float currentTime = 0.0f;
};

class GPUParticleEmitter {
public:
    GPUParticleEmitter();
    ~GPUParticleEmitter() = default;

    void Initialize(ID3D12Device* device, const GPUParticleEmitterData& data);
    void Update(float deltaTime, bool allowEmit = true);
    void Draw(ID3D12GraphicsCommandList* commandList, const Matrix4x4& viewProjection, const Matrix4x4& cameraMatrix, ParticleCommon* particleCommon, ModelManager* modelManager);

    void Reset();
    void OnLoopCycle();
    void EmitBurst(uint32_t count);

    // データ設定 & 取得
    GPUParticleEmitterData& GetData() { return data_; }
    const GPUParticleEmitterData& GetData() const { return data_; }
    void SetData(const GPUParticleEmitterData& data);

    uint32_t GetActiveParticleCount() const { return numActiveParticles_; }
    float GetCurrentTime() const { return systemTime_; }
    void SetCurrentTime(float t);

private:
    void ReallocateGpuResources(ID3D12Device* device, uint32_t maxCount);
    void SpawnParticle();
    void TransferToGPU(const Matrix4x4& viewProjection, const Matrix4x4& cameraMatrix);
    Vector4 EvaluateColor(const GPUParticleInstance& p, float normalizedTime) const;

private:
    ID3D12Device* device_ = nullptr;
    GPUParticleEmitterData data_;

    std::vector<GPUParticleInstance> particles_;
    uint32_t numActiveParticles_ = 0;

    float systemTime_ = 0.0f;
    float spawnTimer_ = 0.0f;
    std::vector<bool> burstTriggered_;

    // GPU Instancing バッファ
    Microsoft::WRL::ComPtr<ID3D12Resource> instancingResource_;
    D3D12_CPU_DESCRIPTOR_HANDLE instancingSrvHandleCPU_{};
    D3D12_GPU_DESCRIPTOR_HANDLE instancingSrvHandleGPU_{};
    ParticleForGPU* mappedInstancingData_ = nullptr;
    uint32_t allocatedCapacity_ = 0;

    // マテリアル用リソース
    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
    Material* mappedMaterial_ = nullptr;

    // 乱数エンジン
    std::mt19937 randomEngine_;
};
