#pragma once
#include "BaseBlock.h"
#include <string>

class MovingBlock : public BaseBlock {
public:
    MovingBlock(MapChip2D* map, int chipX, int chipY);
    ~MovingBlock() override = default;

    void Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) override;
    void Update() override;
    
    bool IsSolid() const override { return true; }
    bool IsMoving() const override { return true; }
    Vector3 GetVelocity() const override { return currentVelocity_; }
    void OnPlayerStand(Player2D* player) override;
    void SetProperties(const nlohmann::json& properties) override;

#ifdef USE_IMGUI
    void DrawImGui() override;
#endif

private:
    float startX_ = 0.0f;
    float startY_ = 0.0f;
    Vector3 prevPosition_ = {0.0f, 0.0f, 0.0f};
    Vector3 deltaPosition_ = {0.0f, 0.0f, 0.0f};
    Vector3 currentVelocity_ = {0.0f, 0.0f, 0.0f};

    std::string moveAxis_ = "X"; // "X" or "Y"
    float moveRange_ = 3.0f;
    float moveSpeed_ = 2.0f;
    float timer_ = 0.0f;
};
