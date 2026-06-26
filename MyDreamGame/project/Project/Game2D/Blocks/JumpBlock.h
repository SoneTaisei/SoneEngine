#pragma once
#include "BaseBlock.h"

class JumpBlock : public BaseBlock {
public:
    using BaseBlock::BaseBlock;
    void Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) override;
    
    // 当たり判定：すり抜けないようにする
    bool IsSolid() const override { return true; }

    // プレイヤーが上に乗った際の処理
    void OnPlayerStand() override;

    // プレイヤーと接触した際の処理（横から触れた場合なども跳ねさせるか？今回は乗った時を優先だが、念のため両方で判定する）
    void OnCollision(Player2D* player) override;

    // Jsonプロパティの受け取り
    void SetProperties(const nlohmann::json& properties) override;

private:
    float jumpVelocity_ = 15.0f; // ジャンプの威力
};
