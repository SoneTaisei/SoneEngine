#pragma once
#include "Component/ColliderComponent.h"

/// <summary>
/// ボックスコライダー（直方体・AABB/OBBの当たり判定）
/// </summary>
class BoxColliderComponent : public ColliderComponent {
public:
    BoxColliderComponent();
    ~BoxColliderComponent() override = default;
    ColliderType GetColliderType() const override { return isOBB_ ? ColliderType::kOBB : ColliderType::kAABB; }
    void SetIsOBB(bool isOBB) { isOBB_ = isOBB; }
    // Box専用のアクセサをここへ移動
    void SetBoxSize(const Vector3 &size) { boxSize_ = size; }
    void SetBoxOffset(const Vector3 &offset) { boxOffset_ = offset; }
    Vector3 GetBoxSize() const { return boxSize_; }
    Vector3 GetBoxOffset() const { return boxOffset_; }
    void DisplayImGui() override;
    AABB2D GetAABB() const override;
    void DrawDebug(class LineRenderer *lineRenderer, const Vector4 &color) const override; // 後で追加
private:
    Vector3 boxSize_{1.0f, 1.0f, 1.0f};
    Vector3 boxOffset_{0.0f, 0.0f, 0.0f};
    bool isOBB_ = false;
};
