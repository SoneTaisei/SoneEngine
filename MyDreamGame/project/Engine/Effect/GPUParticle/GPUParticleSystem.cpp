#include "GPUParticleSystem.h"
#include <algorithm>
#include <cmath>

void GPUParticleSystem::Initialize(ID3D12Device* device) {
    device_ = device;

    // デフォルトのシステム構成を作成
    systemData_.systemName = "NewEffect";
    systemData_.duration = 3.0f;
    systemData_.isLoop = true;
    systemData_.emitters.clear();

    // デフォルトエミッター1: スプライト閃光
    GPUParticleEmitterData defaultEmitter;
    defaultEmitter.name = "Flash";
    defaultEmitter.renderType = GPUParticleRenderType::Sprite;
    defaultEmitter.billboardType = GPUParticleBillboardType::AllAxis;
    defaultEmitter.blendMode = BlendMode::kBlendModeAdd;
    defaultEmitter.maxParticles = 500;
    defaultEmitter.spawnRate = 40.0f;
    defaultEmitter.lifetimeMin = 0.8f;
    defaultEmitter.lifetimeMax = 1.5f;
    defaultEmitter.initialSpeedMin = 1.0f;
    defaultEmitter.initialSpeedMax = 3.0f;
    defaultEmitter.initialScaleMin = { 0.4f, 0.4f, 0.4f };
    defaultEmitter.initialScaleMax = { 0.8f, 0.8f, 0.8f };
    defaultEmitter.startColor = { 1.0f, 0.8f, 0.3f, 1.0f };
    defaultEmitter.endColor = { 1.0f, 0.2f, 0.0f, 0.0f };
    systemData_.emitters.push_back(defaultEmitter);

    RebuildEmitters();
    Restart();
    isPlaying_ = false;
}

void GPUParticleSystem::Initialize(ID3D12Device* device, const GPUParticleSystemData& data) {
    device_ = device;
    systemData_ = data;
    RebuildEmitters();
    Restart();
    isPlaying_ = false;
}

void GPUParticleSystem::RebuildEmitters() {
    emitters_.clear();
    if (!device_) return;

    for (const auto& emitterData : systemData_.emitters) {
        auto emitter = std::make_unique<GPUParticleEmitter>();
        emitter->Initialize(device_, emitterData);
        emitter->SetPosition(position_);
        emitters_.push_back(std::move(emitter));
    }
}

void GPUParticleSystem::SetPosition(const Vector3& position) {
    position_ = position;
    for (auto& emitter : emitters_) {
        if (emitter) {
            emitter->SetPosition(position_);
        }
    }
}

void GPUParticleSystem::SyncDataFromEmitters() {
    systemData_.emitters.clear();
    for (const auto& emitter : emitters_) {
        if (emitter) {
            systemData_.emitters.push_back(emitter->GetData());
        }
    }
}

void GPUParticleSystem::SetData(const GPUParticleSystemData& data) {
    systemData_ = data;
    RebuildEmitters();
    Restart();
    isPlaying_ = false;
}

void GPUParticleSystem::Restart() {
    systemTime_ = 0.0f;
    for (auto& emitter : emitters_) {
        if (emitter) emitter->Reset();
    }
}

void GPUParticleSystem::TriggerBurstAll() {
    for (auto& emitter : emitters_) {
        if (emitter) {
            uint32_t burstCount = 20;
            if (!emitter->GetData().bursts.empty()) {
                burstCount = emitter->GetData().bursts[0].count;
            }
            emitter->EmitBurst(burstCount);
        }
    }
}

void GPUParticleSystem::Update(float deltaTime) {
    if (!isPlaying_) return;

    systemTime_ += deltaTime;

    if (systemData_.duration > 0.0f && systemTime_ >= systemData_.duration) {
        if (systemData_.isLoop) {
            // シームレスループ: 既存の生存パーティクルを消去せず、時間をラップして次周の発生へ継続
            systemTime_ = std::fmod(systemTime_, systemData_.duration);
            for (auto& emitter : emitters_) {
                if (emitter && emitter->GetData().isLoop) {
                    emitter->OnLoopCycle();
                }
            }
        } else {
            // ループOFF: システム時間をdurationでクランプ
            systemTime_ = systemData_.duration;
            // 生存パーティクルがすべて消滅したら自動停止
            if (GetTotalActiveParticles() == 0) {
                isPlaying_ = false;
            }
        }
    }

    // ソロモードのエミッターが存在するかチェック
    bool hasSolo = false;
    for (const auto& emitter : emitters_) {
        if (emitter && emitter->GetData().solo) {
            hasSolo = true;
            break;
        }
    }

    for (auto& emitter : emitters_) {
        if (!emitter) continue;
        if (hasSolo && !emitter->GetData().solo) continue;

        // システムが非ループの場合、全体のdurationに達していたら新規発生は停止
        bool allowEmit = systemData_.isLoop || (systemTime_ < systemData_.duration);
        emitter->Update(deltaTime, allowEmit);
    }
}

void GPUParticleSystem::Draw(ID3D12GraphicsCommandList* commandList, const Matrix4x4& viewProjection, const Matrix4x4& cameraMatrix, ParticleCommon* particleCommon, ModelManager* modelManager) {
    bool hasSolo = false;
    for (const auto& emitter : emitters_) {
        if (emitter && emitter->GetData().solo) {
            hasSolo = true;
            break;
        }
    }

    for (auto& emitter : emitters_) {
        if (!emitter) continue;
        if (hasSolo && !emitter->GetData().solo) continue;
        emitter->Draw(commandList, viewProjection, cameraMatrix, particleCommon, modelManager);
    }
}

GPUParticleEmitter* GPUParticleSystem::GetEmitter(size_t index) {
    if (index < emitters_.size()) return emitters_[index].get();
    return nullptr;
}

const GPUParticleEmitter* GPUParticleSystem::GetEmitter(size_t index) const {
    if (index < emitters_.size()) return emitters_[index].get();
    return nullptr;
}

void GPUParticleSystem::AddEmitter(const GPUParticleEmitterData& emitterData) {
    if (!device_) return;
    auto emitter = std::make_unique<GPUParticleEmitter>();
    emitter->Initialize(device_, emitterData);
    emitter->SetPosition(position_);
    emitters_.push_back(std::move(emitter));
    SyncDataFromEmitters();
}

void GPUParticleSystem::DuplicateEmitter(size_t index) {
    if (index >= emitters_.size() || !device_) return;
    GPUParticleEmitterData copyData = emitters_[index]->GetData();
    copyData.name += " (Copy)";
    auto emitter = std::make_unique<GPUParticleEmitter>();
    emitter->Initialize(device_, copyData);
    emitter->SetPosition(position_);
    emitters_.insert(emitters_.begin() + index + 1, std::move(emitter));
    SyncDataFromEmitters();
}

void GPUParticleSystem::RemoveEmitter(size_t index) {
    if (index >= emitters_.size()) return;
    emitters_.erase(emitters_.begin() + index);
    SyncDataFromEmitters();
}

void GPUParticleSystem::MoveEmitterUp(size_t index) {
    if (index > 0 && index < emitters_.size()) {
        std::swap(emitters_[index], emitters_[index - 1]);
        SyncDataFromEmitters();
    }
}

void GPUParticleSystem::MoveEmitterDown(size_t index) {
    if (index + 1 < emitters_.size()) {
        std::swap(emitters_[index], emitters_[index + 1]);
        SyncDataFromEmitters();
    }
}

bool GPUParticleSystem::SaveToFile(const std::string& filePath) {
    SyncDataFromEmitters();
    return SaveParticleSystemToJson(systemData_, filePath);
}

bool GPUParticleSystem::LoadFromFile(const std::string& filePath) {
    GPUParticleSystemData loadedData;
    if (LoadParticleSystemFromJson(loadedData, filePath)) {
        SetData(loadedData);
        return true;
    }
    return false;
}

uint32_t GPUParticleSystem::GetTotalActiveParticles() const {
    uint32_t total = 0;
    for (const auto& emitter : emitters_) {
        if (emitter) total += emitter->GetActiveParticleCount();
    }
    return total;
}

uint32_t GPUParticleSystem::GetTotalMaxParticles() const {
    uint32_t total = 0;
    for (const auto& emitter : emitters_) {
        if (emitter) total += emitter->GetData().maxParticles;
    }
    return total;
}
