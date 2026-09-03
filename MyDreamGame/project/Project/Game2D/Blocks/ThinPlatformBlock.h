#pragma once
#include "BaseBlock.h"

/// <summary>
/// 細い足場（板）
/// - プレイヤー：上にだけ乗れる（下からはすり抜けて上がれる）。判定は片方向床と同じ
/// - 鎖・宝石：ソリッドではないので素通りする（振り回した鎖や落とした鎖が板に引っかからない）
/// 見た目はチップ上端に貼り付いた薄い木の板
/// </summary>
class ThinPlatformBlock : public BaseBlock {
public:
    using BaseBlock::BaseBlock;
    void Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) override;
    bool IsOneWay() const override { return true; }
    // 板の上に立っている時は鎖を回せる（板は鎖が素通りするので、下の空間で自由に振れる）
    bool AllowsChainSpin() const override { return true; }
    void SetProperties(const nlohmann::json& properties) override;

private:
    float thickness_ = 0.2f; // 板の厚み（チップ高さに対する割合）
};
