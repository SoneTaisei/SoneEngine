#pragma once
#include "Game2D/Blocks/BaseBlock.h"

class GoalBlock : public BaseBlock {
public:
    using BaseBlock::BaseBlock;
    void Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) override;
    void Update() override;
    void Reset() override;
    void OnCollision(Player2D* player) override;
    void SetProperties(const nlohmann::json& properties) override;

#ifdef USE_IMGUI
    void DrawImGui() override;
#endif

private:
    Vector3 basePosition_ = { 0.0f, 0.0f, 0.0f };
    float floatAmplitude_ = 0.25f; // 荳贋ｸ九・豬ｮ驕翫・謖ｯ蟷・
    float floatSpeed_ = 2.0f;      // 豬ｮ驕翫・騾溷ｺｦ繝ｻ蜻ｨ豕｢謨ｰ
    float floatTimer_ = 0.0f;      // 豬ｮ驕顔畑繧ｿ繧､繝槭・
};

