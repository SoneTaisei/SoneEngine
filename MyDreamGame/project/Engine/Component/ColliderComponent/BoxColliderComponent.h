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

    void SetBoxSize(const Vector3& size) override { boxSize_ = size; }
    Vector3 GetBoxSize() const override { return boxSize_; }

    void SetBoxOffset(const Vector3& offset) override { boxOffset_ = offset; }
    Vector3 GetBoxOffset() const override { return boxOffset_; }
};
