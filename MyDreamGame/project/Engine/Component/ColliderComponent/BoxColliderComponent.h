#pragma once
#include "Component/ColliderComponent.h"

/// <summary>
/// ボックスコライダー（直方体・AABB/OBBの当たり判定）
/// </summary>
class BoxColliderComponent : public ColliderComponent {
public:
    BoxColliderComponent();
    ~BoxColliderComponent() override = default;

    ColliderType GetColliderType() const override { return ColliderType::kAABB; }

    void DisplayImGui() override;
    AABB2D GetAABB() const override;
};
