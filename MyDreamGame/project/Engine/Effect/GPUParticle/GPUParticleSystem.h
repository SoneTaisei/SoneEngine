#pragma once
#include <d3d12.h>
#include <memory>
#include <vector>
#include <string>
#include "GPUParticleData.h"
#include "GPUParticleEmitter.h"

class GPUParticleSystem {
public:
    GPUParticleSystem() = default;
    ~GPUParticleSystem() = default;

    void Initialize(ID3D12Device* device);
    void Initialize(ID3D12Device* device, const GPUParticleSystemData& data);

    void Update(float deltaTime);
    void Draw(ID3D12GraphicsCommandList* commandList, const Matrix4x4& viewProjection, const Matrix4x4& cameraMatrix, ParticleCommon* particleCommon, ModelManager* modelManager);

    // 再生コントロール
    void Play() {
        if (!systemData_.isLoop && systemData_.duration > 0.0f && systemTime_ >= systemData_.duration) {
            Restart();
        }
        isPlaying_ = true;
    }
    void Pause() { isPlaying_ = false; }
    void Restart();
    void TriggerBurstAll();

    // 指定位置でリスタートして再生
    void PlayAt(const Vector3& position) {
        SetPosition(position);
        Restart();
        Play();
    }

    // 位置設定
    void SetPosition(const Vector3& position);
    const Vector3& GetPosition() const { return position_; }

    bool IsPlaying() const { return isPlaying_; }
    float GetCurrentTime() const { return systemTime_; }
    void SetCurrentTime(float t) {
        systemTime_ = t;
        for (auto& emitter : emitters_) {
            if (emitter) emitter->SetCurrentTime(t);
        }
    }

    // エミッター管理
    size_t GetEmitterCount() const { return emitters_.size(); }
    GPUParticleEmitter* GetEmitter(size_t index);
    const GPUParticleEmitter* GetEmitter(size_t index) const;

    void AddEmitter(const GPUParticleEmitterData& emitterData);
    void DuplicateEmitter(size_t index);
    void RemoveEmitter(size_t index);
    void MoveEmitterUp(size_t index);
    void MoveEmitterDown(size_t index);

    // システムデータ
    GPUParticleSystemData& GetData() { return systemData_; }
    const GPUParticleSystemData& GetData() const { return systemData_; }
    void SetData(const GPUParticleSystemData& data);

    // JSON I/O
    bool SaveToFile(const std::string& filePath);
    bool LoadFromFile(const std::string& filePath);

    // 統計
    uint32_t GetTotalActiveParticles() const;
    uint32_t GetTotalMaxParticles() const;

private:
    void SyncDataFromEmitters();
    void RebuildEmitters();

private:
    ID3D12Device* device_ = nullptr;
    GPUParticleSystemData systemData_;
    std::vector<std::unique_ptr<GPUParticleEmitter>> emitters_;
    Vector3 position_ = { 0.0f, 0.0f, 0.0f };

    float systemTime_ = 0.0f;
    bool isPlaying_ = false;
};
