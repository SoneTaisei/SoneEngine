#pragma once
#include "BaseBlock.h"

class GPUParticleSystem;

class DeathBlock : public BaseBlock {
public:
    DeathBlock(MapChip2D* map, int chipX, int chipY);
    ~DeathBlock() override;

    void Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) override;
    void Update() override;
    void DrawParticle(
        ID3D12GraphicsCommandList* commandList,
        const Matrix4x4& viewProjection,
        const Matrix4x4& cameraMatrix,
        ParticleCommon* particleCommon,
        ModelManager* modelManager) override;

    bool IsSolid() const override { return true; }
    void OnCollision(Player2D* player) override;
    void OnPlayerStand(Player2D* player) override;
    void OnPlayerTouch(Player2D* player) override;

    void Reset() override;

private:
    std::unique_ptr<GPUParticleSystem> thunderParticle_;
};
