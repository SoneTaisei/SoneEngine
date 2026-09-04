#pragma once
#include "BaseBlock.h"
#include <vector>

/// <summary>
/// 鎖アイテム。触れると鎖が units 本増える（既定 1）。本数は上に並ぶ点で分かる
/// </summary>
class ChainItemBlock : public BaseBlock {
public:
    using BaseBlock::BaseBlock;

    void Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) override;
    void Draw() override;

    // プレイヤーとぶつかったときの処理
    void OnCollision(Player2D* player) override;

    // 壁ではないのですり抜けられるようにする
    bool IsSolid() const override { return false; }

    void SetProperties(const nlohmann::json& properties) override;
    int GetUnits() const { return units_; }

private:
    void SyncPips();

    int units_ = 1;
    float startX_ = 0.0f;
    float startY_ = 0.0f;
    float width_ = 1.0f;
    float height_ = 1.0f;
    ID3D12Device* device_ = nullptr;   // 非所有
    Primitive* boxPrimitive_ = nullptr; // 非所有
    std::vector<std::unique_ptr<GameObject>> pips_; // 本数の表示（本体の上に並ぶ点）
};
