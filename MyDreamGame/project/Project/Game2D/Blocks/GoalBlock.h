#pragma once
#include "BaseBlock.h"

class GoalBlock : public BaseBlock {
public:
    using BaseBlock::BaseBlock;
    void Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) override;
    void OnCollision(Player2D* player) override;

    // 台座として上に乗れるようにソリッド化
    bool IsSolid() const override { return true; }

    // 台座の見た目（高さ半分）に合わせた当たり判定
    AABB2D GetAABB() const override;

    // 台座上面のワールドY座標
    float GetTopY() const { return worldY_; } // 底面(worldY_ - height_*0.5f) + 高さ半分(height_*0.5f) = worldY_
    Vector3 GetPosition() const { return { worldX_, worldY_, 0.0f }; }
    float GetWidth() const { return width_; }

    // プレイヤーと宝石の両方が台座の上に乗っているか判定
    bool CheckClearCondition(const Vector3& playerPos, float playerHalfHeight, const Vector3& gemPos) const;

private:
    float worldX_ = 0.0f;
    float worldY_ = 0.0f;
    float width_ = 1.0f;
    float height_ = 1.0f;
};
