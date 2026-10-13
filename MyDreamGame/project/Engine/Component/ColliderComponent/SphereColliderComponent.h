#pragma once
#include "Component/ColliderComponent.h"

/// <summary>
/// スフィアコライダー（球体・円形の当たり判定）
/// </summary>
class SphereColliderComponent : public ColliderComponent {
public:
    SphereColliderComponent();
    ~SphereColliderComponent() override = default;

    ColliderType GetColliderType() const override { return ColliderType::kSphere; }

    void DisplayImGui() override;
    AABB2D GetAABB() const override;
};
