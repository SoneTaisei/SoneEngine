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

    // Sphere専用のアクセサをここへ移動
    void SetSphereRadius(float radius) { sphereRadius_ = radius; }
    void SetSphereOffset(const Vector3 &offset) { sphereOffset_ = offset; }
    float GetSphereRadius() const { return sphereRadius_; }
    Vector3 GetSphereOffset() const { return sphereOffset_; }

    void DisplayImGui() override;
    AABB2D GetAABB() const override;
    void DrawDebug(class LineRenderer *lineRenderer, const Vector4 &color) const override; // 後で追加

private:
    float sphereRadius_ = 1.0f;
    Vector3 sphereOffset_{0.0f, 0.0f, 0.0f};
};
