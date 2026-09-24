#pragma once
#include "BaseBlock.h"

class GoalBlock : public BaseBlock {
public:
    using BaseBlock::BaseBlock;
    void Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) override;
    void Update() override;
    void Reset() override;
    void OnCollision(Player2D* player) override;
    void SetProperties(const nlohmann::json& properties) override;

#ifdef USE_IMGUI
    void DrawImGui() override;
#endif

private:
    Vector3 basePosition_ = { 0.0f, 0.0f, 0.0f };
    float floatAmplitude_ = 0.25f; // 上下の浮遊の振幅
    float floatSpeed_ = 2.0f;      // 浮遊の速度・周波数
    float floatTimer_ = 0.0f;      // 浮遊用タイマー
};

