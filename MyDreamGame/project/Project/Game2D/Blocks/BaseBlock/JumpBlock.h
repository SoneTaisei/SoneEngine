#pragma once
#include "Game2D/Blocks/BaseBlock.h"

class JumpBlock : public BaseBlock {
public:
    enum class BounceDirection {
        kUp,
        kDown,
        kLeft,
        kRight,
        kCenter
    };

    using BaseBlock::BaseBlock;
    void Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) override;
    
    // 蠖薙◆繧雁愛螳夲ｼ壹☆繧頑栢縺代↑縺・ｈ縺・↓縺吶ｋ
    bool IsSolid() const override { return true; }

    // 豈弱ヵ繝ｬ繝ｼ繝縺ｮ譖ｴ譁ｰ・医ヰ繧ｦ繝ｳ繝峨い繝九Γ繝ｼ繧ｷ繝ｧ繝ｳ縺ｮ繧ｷ繝溘Η繝ｬ繝ｼ繧ｷ繝ｧ繝ｳ・・
    void Update() override;

    // 繝ｪ繧ｻ繝・ヨ蜃ｦ逅・ｼ医い繝九Γ繝ｼ繧ｷ繝ｧ繝ｳ迥ｶ諷九ｒ謌ｻ縺呻ｼ・
    void Reset() override;

    // 繧ｳ繝ｩ繧､繝繝ｼ繝ｻ蛻､螳夂畑AABB・医い繝九Γ繝ｼ繧ｷ繝ｧ繝ｳ螟牙ｽ｢縺ｮ蠖ｱ髻ｿ繧貞女縺代↑縺・崋螳哂ABB・・
    AABB2D GetAABB() const override;

    // 繝励Ξ繧､繝､繝ｼ縺御ｸ翫↓荵励▲縺滄圀縺ｮ蜃ｦ逅・
    void OnPlayerStand() override;

    // 繝励Ξ繧､繝､繝ｼ縺ｨ謗･隗ｦ縺励◆髫帙・蜃ｦ逅・
    void OnCollision(Player2D* player) override;

    // Json繝励Ο繝代ユ繧｣縺ｮ蜿励￠蜿悶ｊ
    void SetProperties(const nlohmann::json& properties) override;

    // 險ｭ鄂ｮ迥ｶ豕√°繧峨・縺ｭ縺ｮ蜷代″繧貞愛螳・
    BounceDirection DetermineFacingDirection() const;
    // 蜷代″縺ｫ蠢懊§縺飮霆ｸ蝗櫁ｻ｢隗抵ｼ医Λ繧ｸ繧｢繝ｳ・峨ｒ蜿門ｾ・
    float GetRotationZForDirection(BounceDirection dir) const;

    // 繝舌え繝ｳ繝峨い繝九Γ繝ｼ繧ｷ繝ｧ繝ｳ縺ｮ繝医Μ繧ｬ繝ｼ
    void TriggerBounce(BounceDirection dir);

#ifdef USE_IMGUI
    void DrawImGui() override;
#endif

private:
    float jumpVelocityVertical_ = 15.0f; // 邵ｦ繧ｸ繝｣繝ｳ繝励・螽∝鴨
    float jumpVelocityHorizontal_ = 15.0f; // 讓ｪ繧ｸ繝｣繝ｳ繝励・螽∝鴨

    // 繝舌え繝ｳ繝峨い繝九Γ繝ｼ繧ｷ繝ｧ繝ｳ逕ｨ螟画焚・育黄逅・せ繝励Μ繝ｳ繧ｰ繧ｷ繝溘Η繝ｬ繝ｼ繧ｷ繝ｧ繝ｳ・・
    bool isBouncing_ = false;
    float bounceAmount_ = 0.0f;     // 荳ｻ霆ｸ縺ｮ螟我ｽ搾ｼ医・繧､繝翫せ縺ｧ邵ｮ縺ｿ縲√・繝ｩ繧ｹ縺ｧ莨ｸ縺ｳ・・
    float bounceVelocity_ = 0.0f;   // 荳ｻ霆ｸ縺ｮ螟我ｽ埼溷ｺｦ
    BounceDirection bounceDir_ = BounceDirection::kUp;

    // 繝吶・繧ｹTransform・亥・譛滄・鄂ｮ蝓ｺ貅厄ｼ・
    Vector3 basePosition_ = {0.0f, 0.0f, 0.0f};
    Vector3 baseScale_ = {1.0f, 1.0f, 1.0f};
    Vector3 baseRotation_ = {0.0f, 0.0f, 0.0f};
    AABB2D baseAABB_{};
    bool baseCaptured_ = false;

    // 繧｢繝九Γ繝ｼ繧ｷ繝ｧ繝ｳ險ｭ螳壹ヱ繝ｩ繝｡繝ｼ繧ｿ
    float stiffness_ = 280.0f;      // 繝舌ロ縺ｮ遑ｬ縺包ｼ亥捉豕｢謨ｰ・・
    float damping_ = 14.0f;         // 貂幄｡ｰ邇・ｼ井ｽ咎渊縺ｮ髟ｷ縺包ｼ・
    float initialSquash_ = -0.42f;  // 逋ｺ蜍墓凾縺ｮ邵ｮ縺ｿ螟我ｽ・
    float initialVelocity_ = 12.0f; // 逋ｺ蜍墓凾縺ｮ莨ｸ縺ｳ繧医≧縺ｨ縺吶ｋ蛻晞・
    float crossScaleRatio_ = 0.35f; // 蝙ら峩霆ｸ縺ｮ諡｡邵ｮ豈費ｼ・quash & Stretch・・
};
