#include "GPUParticleEmitter.h"
#include "Graphics/TextureManager.h"
#include "Renderer/SrvManager.h"
#include "Core/Utility/TransformFunctions.h"
#include <cmath>
#include <numbers>
#include <algorithm>
#include <filesystem>

GPUParticleEmitter::GPUParticleEmitter() {
    std::random_device rd;
    randomEngine_.seed(rd());
}

void GPUParticleEmitter::Initialize(ID3D12Device* device, const GPUParticleEmitterData& data) {
    device_ = device;
    data_ = data;
    ReallocateGpuResources(device_, data_.maxParticles);
    Reset();
}

void GPUParticleEmitter::SetData(const GPUParticleEmitterData& data) {
    bool needRealloc = (data.maxParticles != data_.maxParticles);
    data_ = data;
    if (needRealloc && device_) {
        ReallocateGpuResources(device_, data_.maxParticles);
    }
}

void GPUParticleEmitter::ReallocateGpuResources(ID3D12Device* device, uint32_t maxCount) {
    if (maxCount == 0) maxCount = 1;

    instancingResource_ = CreateBufferResource(device, sizeof(ParticleForGPU) * maxCount);
    instancingResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedInstancingData_));

    SrvManager::GetInstance()->Allocate(&instancingSrvHandleCPU_, &instancingSrvHandleGPU_);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Buffer.FirstElement = 0;
    srvDesc.Buffer.NumElements = maxCount;
    srvDesc.Buffer.StructureByteStride = sizeof(ParticleForGPU);
    srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

    device->CreateShaderResourceView(instancingResource_.Get(), &srvDesc, instancingSrvHandleCPU_);

    if (!materialResource_) {
        materialResource_ = CreateBufferResource(device, sizeof(Material));
        materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedMaterial_));
        if (mappedMaterial_) {
            mappedMaterial_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
            mappedMaterial_->uvTransform = TransformFunctions::MakeIdentity4x4();
        }
    }

    particles_.resize(maxCount);
    allocatedCapacity_ = maxCount;
}

void GPUParticleEmitter::Reset() {
    systemTime_ = 0.0f;
    spawnTimer_ = 0.0f;
    numActiveParticles_ = 0;
    burstTriggered_.assign(data_.bursts.size(), false);
}

void GPUParticleEmitter::OnLoopCycle() {
    if (!data_.isLoop) return; // エミッター自身が非ループ設定の場合は周回時にも再発火させない
    systemTime_ = 0.0f;
    spawnTimer_ = 0.0f;
    burstTriggered_.assign(data_.bursts.size(), false);
    // ※ numActiveParticles_ はリセットしない！既存の生存パーティクルをシームレスに継続描画・更新させる
}

void GPUParticleEmitter::SetCurrentTime(float t) {
    systemTime_ = t;
    float activeTime = systemTime_ - data_.startDelay;
    if (burstTriggered_.size() != data_.bursts.size()) {
        burstTriggered_.resize(data_.bursts.size(), false);
    }
    for (size_t i = 0; i < data_.bursts.size(); ++i) {
        burstTriggered_[i] = (activeTime >= data_.bursts[i].time);
    }
}

void GPUParticleEmitter::EmitBurst(uint32_t count) {
    for (uint32_t i = 0; i < count; ++i) {
        SpawnParticle();
    }
}

void GPUParticleEmitter::SpawnParticle() {
    if (numActiveParticles_ >= data_.maxParticles) return;

    GPUParticleInstance& p = particles_[numActiveParticles_];

    std::uniform_real_distribution<float> dist01(0.0f, 1.0f);
    std::uniform_real_distribution<float> distMinus1To1(-1.0f, 1.0f);

    // 1. 発生位置 (Shape)
    Vector3 pos = { 0.0f, 0.0f, 0.0f };
    switch (data_.shape) {
    case GPUParticleSpawnShape::Point:
        pos = { 0.0f, 0.0f, 0.0f };
        break;
    case GPUParticleSpawnShape::Sphere: {
        float theta = dist01(randomEngine_) * 2.0f * std::numbers::pi_v<float>;
        float phi = std::acos(distMinus1To1(randomEngine_));
        float r = std::cbrt(dist01(randomEngine_)) * data_.shapeRadius;
        pos.x = r * std::sin(phi) * std::cos(theta);
        pos.y = r * std::sin(phi) * std::sin(theta);
        pos.z = r * std::cos(phi);
        break;
    }
    case GPUParticleSpawnShape::Box: {
        pos.x = distMinus1To1(randomEngine_) * data_.shapeBoxSize.x * 0.5f;
        pos.y = distMinus1To1(randomEngine_) * data_.shapeBoxSize.y * 0.5f;
        pos.z = distMinus1To1(randomEngine_) * data_.shapeBoxSize.z * 0.5f;
        break;
    }
    case GPUParticleSpawnShape::Cone: {
        float theta = dist01(randomEngine_) * 2.0f * std::numbers::pi_v<float>;
        float r = std::sqrt(dist01(randomEngine_)) * data_.shapeConeRadius;
        pos.x = r * std::cos(theta);
        pos.y = 0.0f;
        pos.z = r * std::sin(theta);
        break;
    }
    case GPUParticleSpawnShape::Ring: {
        float theta = dist01(randomEngine_) * 2.0f * std::numbers::pi_v<float>;
        pos.x = data_.shapeRadius * std::cos(theta);
        pos.y = 0.0f;
        pos.z = data_.shapeRadius * std::sin(theta);
        break;
    }
    }
    p.position = { pos.x + position_.x, pos.y + position_.y, pos.z + position_.z };

    // 2. 速度ベクトル
    Vector3 baseDir = data_.initialVelocityDir;
    float len = std::sqrt(baseDir.x * baseDir.x + baseDir.y * baseDir.y + baseDir.z * baseDir.z);
    if (len > 0.0001f) {
        baseDir = { baseDir.x / len, baseDir.y / len, baseDir.z / len };
    } else {
        baseDir = { 0.0f, 1.0f, 0.0f };
    }

    // 散開角度
    float spreadRad = (data_.velocitySpread * std::numbers::pi_v<float>) / 180.0f;
    Vector3 randDir = { distMinus1To1(randomEngine_), distMinus1To1(randomEngine_), distMinus1To1(randomEngine_) };
    float rLen = std::sqrt(randDir.x * randDir.x + randDir.y * randDir.y + randDir.z * randDir.z);
    if (rLen > 0.0001f) {
        randDir = { randDir.x / rLen, randDir.y / rLen, randDir.z / rLen };
    }

    float spreadAmount = std::sin(spreadRad * 0.5f);
    Vector3 finalDir = {
        baseDir.x * (1.0f - spreadAmount) + randDir.x * spreadAmount,
        baseDir.y * (1.0f - spreadAmount) + randDir.y * spreadAmount,
        baseDir.z * (1.0f - spreadAmount) + randDir.z * spreadAmount
    };
    float fLen = std::sqrt(finalDir.x * finalDir.x + finalDir.y * finalDir.y + finalDir.z * finalDir.z);
    if (fLen > 0.0001f) finalDir = { finalDir.x / fLen, finalDir.y / fLen, finalDir.z / fLen };

    float speed = data_.initialSpeedMin + (data_.initialSpeedMax - data_.initialSpeedMin) * dist01(randomEngine_);
    p.velocity = { finalDir.x * speed, finalDir.y * speed, finalDir.z * speed };

    // 3. 寿命
    p.lifeTime = data_.lifetimeMin + (data_.lifetimeMax - data_.lifetimeMin) * dist01(randomEngine_);
    if (p.lifeTime <= 0.01f) p.lifeTime = 0.01f;
    p.currentTime = 0.0f;

    // 4. サイズ
    p.initialScale = {
        data_.initialScaleMin.x + (data_.initialScaleMax.x - data_.initialScaleMin.x) * dist01(randomEngine_),
        data_.initialScaleMin.y + (data_.initialScaleMax.y - data_.initialScaleMin.y) * dist01(randomEngine_),
        data_.initialScaleMin.z + (data_.initialScaleMax.z - data_.initialScaleMin.z) * dist01(randomEngine_)
    };
    p.scale = p.initialScale;

    // 5. 回転 & 回転速度
    p.rotate = {
        data_.initialRotateMin.x + (data_.initialRotateMax.x - data_.initialRotateMin.x) * dist01(randomEngine_),
        data_.initialRotateMin.y + (data_.initialRotateMax.y - data_.initialRotateMin.y) * dist01(randomEngine_),
        data_.initialRotateMin.z + (data_.initialRotateMax.z - data_.initialRotateMin.z) * dist01(randomEngine_)
    };
    p.rotateSpeed = {
        data_.rotateSpeedMin.x + (data_.rotateSpeedMax.x - data_.rotateSpeedMin.x) * dist01(randomEngine_),
        data_.rotateSpeedMin.y + (data_.rotateSpeedMax.y - data_.rotateSpeedMin.y) * dist01(randomEngine_),
        data_.rotateSpeedMin.z + (data_.rotateSpeedMax.z - data_.rotateSpeedMin.z) * dist01(randomEngine_)
    };

    // 6. カラー
    p.startColor = data_.startColor;
    p.endColor = data_.endColor;
    p.currentColor = p.startColor;

    numActiveParticles_++;
}

void GPUParticleEmitter::Update(float deltaTime, bool allowEmit) {
    if (!data_.enabled || data_.mute) return;

    systemTime_ += deltaTime;

    // 稼働中のみ新規発生（開始遅延後かつ稼働時間内）
    bool isEmitting = allowEmit && (systemTime_ >= data_.startDelay);
    if (data_.duration > 0.0f && systemTime_ >= (data_.duration + data_.startDelay)) {
        isEmitting = false;
    }

    if (isEmitting) {
        // バーストチェック
        float activeTime = systemTime_ - data_.startDelay;
        for (size_t i = 0; i < data_.bursts.size(); ++i) {
            if (i >= burstTriggered_.size()) burstTriggered_.resize(data_.bursts.size(), false);
            if (!burstTriggered_[i] && activeTime >= data_.bursts[i].time) {
                EmitBurst(data_.bursts[i].count);
                burstTriggered_[i] = true;
            }
        }

        // 定常レート発生
        if (data_.spawnRate > 0.0f) {
            spawnTimer_ += deltaTime;
            float spawnInterval = 1.0f / data_.spawnRate;
            while (spawnTimer_ >= spawnInterval) {
                SpawnParticle();
                spawnTimer_ -= spawnInterval;
            }
        }
    }

    // 生存パーティクルの更新
    for (uint32_t i = 0; i < numActiveParticles_; ) {
        GPUParticleInstance& p = particles_[i];
        p.currentTime += deltaTime;

        if (p.currentTime >= p.lifeTime) {
            // 寿命終了 -> 末尾要素と入れ替えて削除 (O(1))
            particles_[i] = particles_[numActiveParticles_ - 1];
            numActiveParticles_--;
            continue;
        }

        float normTime = std::clamp(p.currentTime / p.lifeTime, 0.0f, 1.0f);

        // 重力 & ドラッグ
        p.velocity.y += data_.gravity * deltaTime;
        if (data_.drag > 0.0f) {
            float dragFactor = (std::max)(0.0f, 1.0f - data_.drag * deltaTime);
            p.velocity.x *= dragFactor;
            p.velocity.y *= dragFactor;
            p.velocity.z *= dragFactor;
        }

        // 位置移動
        p.position.x += p.velocity.x * deltaTime;
        p.position.y += p.velocity.y * deltaTime;
        p.position.z += p.velocity.z * deltaTime;

        // 回転更新
        p.rotate.x += p.rotateSpeed.x * deltaTime;
        p.rotate.y += p.rotateSpeed.y * deltaTime;
        p.rotate.z += p.rotateSpeed.z * deltaTime;

        // サイズ変化 (線形補間)
        float currentScaleFactor = 1.0f + (data_.endScaleFactor - 1.0f) * normTime;
        p.scale = {
            p.initialScale.x * currentScaleFactor,
            p.initialScale.y * currentScaleFactor,
            p.initialScale.z * currentScaleFactor
        };

        // カラー評価
        p.currentColor = EvaluateColor(p, normTime);

        ++i;
    }
}

Vector4 GPUParticleEmitter::EvaluateColor(const GPUParticleInstance& p, float normalizedTime) const {
    if (!data_.useColorGradient || data_.colorKeys.size() < 2) {
        // 通常の startColor -> endColor 線形補間
        return {
            p.startColor.x + (p.endColor.x - p.startColor.x) * normalizedTime,
            p.startColor.y + (p.endColor.y - p.startColor.y) * normalizedTime,
            p.startColor.z + (p.endColor.z - p.startColor.z) * normalizedTime,
            p.startColor.w + (p.endColor.w - p.startColor.w) * normalizedTime
        };
    }

    // グラデーションキー補間
    if (normalizedTime <= data_.colorKeys.front().time) return data_.colorKeys.front().color;
    if (normalizedTime >= data_.colorKeys.back().time) return data_.colorKeys.back().color;

    for (size_t i = 0; i < data_.colorKeys.size() - 1; ++i) {
        const auto& k0 = data_.colorKeys[i];
        const auto& k1 = data_.colorKeys[i + 1];
        if (normalizedTime >= k0.time && normalizedTime <= k1.time) {
            float range = k1.time - k0.time;
            float t = range > 0.0001f ? (normalizedTime - k0.time) / range : 0.0f;
            return {
                k0.color.x + (k1.color.x - k0.color.x) * t,
                k0.color.y + (k1.color.y - k0.color.y) * t,
                k0.color.z + (k1.color.z - k0.color.z) * t,
                k0.color.w + (k1.color.w - k0.color.w) * t
            };
        }
    }
    return data_.colorKeys.back().color;
}

void GPUParticleEmitter::TransferToGPU(const Matrix4x4& viewProjection, const Matrix4x4& cameraMatrix) {
    if (!mappedInstancingData_) return;

    Matrix4x4 billboardMatrix = TransformFunctions::MakeIdentity4x4();

    if (data_.billboardType == GPUParticleBillboardType::AllAxis) {
        // カメラの向きに完全追従 (Y180度反転 + カメラ回転)
        Matrix4x4 backToFrontMatrix = TransformFunctions::MakeRoteYMatrix(std::numbers::pi_v<float>);
        billboardMatrix = TransformFunctions::Multiply(backToFrontMatrix, cameraMatrix);
        billboardMatrix.m[3][0] = 0.0f;
        billboardMatrix.m[3][1] = 0.0f;
        billboardMatrix.m[3][2] = 0.0f;
    } else if (data_.billboardType == GPUParticleBillboardType::YAxis) {
        // Y軸のみ回転
        float camForwardX = cameraMatrix.m[2][0];
        float camForwardZ = cameraMatrix.m[2][2];
        float angleY = std::atan2(camForwardX, camForwardZ);
        billboardMatrix = TransformFunctions::MakeRoteYMatrix(angleY + std::numbers::pi_v<float>);
    }

    for (uint32_t i = 0; i < numActiveParticles_; ++i) {
        const GPUParticleInstance& p = particles_[i];

        Matrix4x4 scaleMat = TransformFunctions::MakeScaleMatrix(p.scale);
        Matrix4x4 rotX = TransformFunctions::MakeRoteXMatrix(p.rotate.x);
        Matrix4x4 rotY = TransformFunctions::MakeRoteYMatrix(p.rotate.y);
        Matrix4x4 rotZ = TransformFunctions::MakeRoteZMatrix(p.rotate.z);
        Matrix4x4 rotMat = TransformFunctions::Multiply(rotX, TransformFunctions::Multiply(rotY, rotZ));

        Matrix4x4 localMat = TransformFunctions::Multiply(scaleMat, rotMat);
        Matrix4x4 stateMat;

        if (data_.billboardType == GPUParticleBillboardType::VelocityStretch) {
            // 進行方向ストレッチ
            float speed = std::sqrt(p.velocity.x * p.velocity.x + p.velocity.y * p.velocity.y + p.velocity.z * p.velocity.z);
            Vector3 dir = (speed > 0.0001f) ? Vector3{ p.velocity.x / speed, p.velocity.y / speed, p.velocity.z / speed } : Vector3{ 0.0f, 1.0f, 0.0f };

            // Y軸を速度方向に揃える回転
            Vector3 up = { 0.0f, 1.0f, 0.0f };
            Vector3 axis = { up.y * dir.z - up.z * dir.y, up.z * dir.x - up.x * dir.z, up.x * dir.y - up.y * dir.x };
            float dot = up.x * dir.x + up.y * dir.y + up.z * dir.z;
            float axisLen = std::sqrt(axis.x * axis.x + axis.y * axis.y + axis.z * axis.z);

            Matrix4x4 alignMat = TransformFunctions::MakeIdentity4x4();
            if (axisLen > 0.0001f) {
                axis = { axis.x / axisLen, axis.y / axisLen, axis.z / axisLen };
                float angle = std::acos(std::clamp(dot, -1.0f, 1.0f));
                // 任意軸回転
                float c = std::cos(angle), s = std::sin(angle), oc = 1.0f - c;
                alignMat.m[0][0] = oc * axis.x * axis.x + c;
                alignMat.m[0][1] = oc * axis.x * axis.y - axis.z * s;
                alignMat.m[0][2] = oc * axis.x * axis.z + axis.y * s;
                alignMat.m[1][0] = oc * axis.x * axis.y + axis.z * s;
                alignMat.m[1][1] = oc * axis.y * axis.y + c;
                alignMat.m[1][2] = oc * axis.y * axis.z - axis.x * s;
                alignMat.m[2][0] = oc * axis.x * axis.z - axis.y * s;
                alignMat.m[2][1] = oc * axis.y * axis.z + axis.x * s;
                alignMat.m[2][2] = oc * axis.z * axis.z + c;
            }

            float stretch = 1.0f + speed * data_.stretchFactor;
            Matrix4x4 stretchScale = TransformFunctions::MakeScaleMatrix({ p.scale.x, p.scale.y * stretch, p.scale.z });
            stateMat = TransformFunctions::Multiply(stretchScale, alignMat);
        } else if (data_.billboardType == GPUParticleBillboardType::None) {
            stateMat = localMat;
        } else {
            stateMat = TransformFunctions::Multiply(localMat, billboardMatrix);
        }

        Matrix4x4 transMat = TransformFunctions::MakeTranslateMatrix(p.position);
        Matrix4x4 worldMat = TransformFunctions::Multiply(stateMat, transMat);
        Matrix4x4 wvpMat = TransformFunctions::Multiply(worldMat, viewProjection);

        mappedInstancingData_[i].World = worldMat;
        mappedInstancingData_[i].WVP = wvpMat;
        mappedInstancingData_[i].color = p.currentColor;
    }
}

void GPUParticleEmitter::Draw(ID3D12GraphicsCommandList* commandList, const Matrix4x4& viewProjection, const Matrix4x4& cameraMatrix, ParticleCommon* particleCommon, ModelManager* modelManager) {
    if (!data_.enabled || data_.mute || numActiveParticles_ == 0 || !commandList || !particleCommon) return;

    TransferToGPU(viewProjection, cameraMatrix);

    particleCommon->SetBlendMode(data_.blendMode);

    uint32_t texHandle = TextureManager::GetInstance()->Load(data_.texturePath);
    D3D12_GPU_DESCRIPTOR_HANDLE gpuTexHandle = TextureManager::GetInstance()->GetGpuHandle(texHandle);

    commandList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootDescriptorTable(1, instancingSrvHandleGPU_);
    commandList->SetGraphicsRootDescriptorTable(2, gpuTexHandle);

    if (data_.renderType == GPUParticleRenderType::Mesh && modelManager) {
        std::filesystem::path p(data_.modelPath);
        std::string dir = p.parent_path().string();
        if (!dir.empty() && dir.back() != '/' && dir.back() != '\\') dir += "/";
        std::string filename = p.filename().string();

        Model* model = modelManager->GetModel(dir, filename);
        if (model && model->GetIndexCount() > 0) {
            D3D12_VERTEX_BUFFER_VIEW vbv = model->GetVertexBufferView();
            D3D12_INDEX_BUFFER_VIEW ibv = model->GetIndexBufferView();
            commandList->IASetVertexBuffers(0, 1, &vbv);
            commandList->IASetIndexBuffer(&ibv);
            commandList->DrawIndexedInstanced(model->GetIndexCount(), numActiveParticles_, 0, 0, 0);
            return;
        }
    }

    // デフォルト（スプライト板ポリゴン）
    commandList->IASetVertexBuffers(0, 1, &particleCommon->GetVertexBufferView());
    commandList->DrawInstanced(particleCommon->GetVertexCount(), numActiveParticles_, 0, 0);
}
