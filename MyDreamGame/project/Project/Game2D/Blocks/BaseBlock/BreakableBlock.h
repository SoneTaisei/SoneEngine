#pragma once
#include "Game2D/Blocks/BaseBlock.h"
#include <vector>
#include <memory>

/// <summary>
/// BreakableBlock - 騾壼ｸｸ譎ゅ・譎ｮ騾壹・繝悶Ο繝・け縺ｨ縺励※讖溯・縺励・
/// 繝繝・す繝･縺ｧ豼遯√＠縺滓凾縺ｮ縺ｿ遐ｴ螢翫＆繧後ｋ繝悶Ο繝・け繧ｯ繝ｩ繧ｹ
/// </summary>
class BreakableBlock : public BaseBlock {
public:
    using BaseBlock::BaseBlock;

    // 蛻晄悄蛹門・逅・ｼ医Δ繝・Ν繝ｻ繝槭ユ繝ｪ繧｢繝ｫ繝ｻ繧ｳ繝ｩ繧､繝繝ｼ繝ｻ遐ｴ迚・ｼ泌・縺ｮ繧ｻ繝・ヨ繧｢繝・・・・
    void Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) override;

    // 豈弱ヵ繝ｬ繝ｼ繝縺ｮ譖ｴ譁ｰ蜃ｦ逅・ｼ育ｴ迚・い繝九Γ繝ｼ繧ｷ繝ｧ繝ｳ繝ｻ蠕ｩ豢ｻ繧ｿ繧､繝槭・縺ｪ縺ｩ・・
    void Update() override;

    // 謠冗判蜃ｦ逅・ｼ亥｣翫ｌ縺ｦ縺・↑縺・凾縺ｯ譛ｬ菴薙∝｣翫ｌ縺ｦ縺・ｋ譎ゅ・遐ｴ迚・ｒ謠冗判・・
    void Draw() override;

    // 蠖薙◆繧雁愛螳壹・諤ｧ雉ｪ・亥｣翫ｌ縺ｦ縺・↑縺・凾縺ｯ騾壼ｸｸ繝悶Ο繝・け縺ｨ蜷後§Solid・・
    bool IsSolid() const override { return !isBroken_; }
    bool IsOneWay() const override { return false; }

    // 繝励Ξ繧､繝､繝ｼ縺梧磁隗ｦ縺励◆迸ｬ髢薙・蜃ｦ逅・ｼ医ム繝・す繝･荳ｭ縺ｪ繧臥ｴ螢奇ｼ・
    void OnCollision(Player2D* player) override;

    // 繝励Ξ繧､繝､繝ｼ縺後ヶ繝ｭ繝・け縺ｮ荳翫↓荵励▲縺ｦ縺・ｋ譎ゅ・豈弱ヵ繝ｬ繝ｼ繝蜃ｦ逅・
    void OnPlayerStand() override;

    // 繝励Ξ繧､繝､繝ｼ縺梧ｨｪ繧・ｸ九°繧芽ｧｦ繧後◆譎ゅ・蜃ｦ逅・
    void OnPlayerTouch() override;

    // 繝悶Ο繝・け縺ｮ遐ｴ螢雁・逅・
    void Break(Player2D* player = nullptr, bool triggerChain = true);

    // 蜊倅ｽ薙ヶ繝ｭ繝・け縺ｮ蜀・Κ遐ｴ螢雁・逅・
    void BreakInternal(Player2D* player, const Vector3& inheritedDashDir = { 1.0f, 0.0f, 0.0f });

    // 繝悶Ο繝・け縺ｮ蠕ｩ豢ｻ蜃ｦ逅・
    void Respawn();

    // JSON繝励Ο繝代ユ繧｣縺ｮ隱ｭ縺ｿ霎ｼ縺ｿ・医お繝・ぅ繧ｿ縺ｮ繧､繝ｳ繧ｹ繝壹け繧ｿ繝ｼ縺九ｉ縺ｮ繝代Λ繝｡繝ｼ繧ｿ險ｭ螳夲ｼ・
    void SetProperties(const nlohmann::json& properties) override;

    // 繧ｹ繝・・繧ｸ蜀埼幕譎ゅ・繝ｪ繝医Λ繧､譎ゅ・迥ｶ諷九Μ繧ｻ繝・ヨ蜃ｦ逅・
    void Reset() override;

    // 迥ｶ諷句叙蠕・
    bool IsBroken() const { return isBroken_; }
    bool IsBreakConnected() const { return breakConnected_; }
    void SetBreakConnected(bool enable) { breakConnected_ = enable; }

    // 謗･隗ｦ髱｢縺ｨ繝繝・す繝･譁ｹ蜷代°繧臥ｴ螢翫☆縺ｹ縺阪°繧貞愛螳壹☆繧九・繝ｫ繝代・髢｢謨ｰ
    bool ShouldBreakFromContact(Player2D* player) const;

#ifdef USE_IMGUI
    // ImGui縺ｫ繧医ｋ繝ｪ繧｢繝ｫ繧ｿ繧､繝繝代Λ繝｡繝ｼ繧ｿ隱ｿ謨ｴ繝ｻ繝・ヰ繝・げ逕ｨUI髢｢謨ｰ
    void DrawImGui() override;
#endif

private:
    struct Debris {
        std::unique_ptr<GameObject> gameObject;
        Vector3 initialScale{ 0.3f, 0.3f, 0.3f };
        Vector3 velocity{ 0.0f, 0.0f, 0.0f };
        Vector3 rotationVelocity{ 0.0f, 0.0f, 0.0f };
        float timer = 0.0f;
        float lifetime = 0.6f;
        bool active = false;
    };

    bool isBroken_ = false;               // 遐ｴ螢顔憾諷九ヵ繝ｩ繧ｰ
    bool requireDirectionalDash_ = true;  // 謗･隗ｦ髱｢縺ｫ蠢懊§縺滓婿蜷代・繝繝・す繝･縺ｮ縺ｿ縺ｧ螢翫ｌ繧九°
    bool breakConnected_ = true;          // 騾｣邨舌＠縺ｦ縺・ｋ繝悶Ο繝・け繧偵∪縺ｨ繧√※遐ｴ螢翫☆繧九°
    float respawnTimer_ = 0.0f;           // 閾ｪ蜍募ｾｩ豢ｻ繧ｫ繧ｦ繝ｳ繝医ち繧､繝槭・
    float respawnTime_ = 0.0f;            // 閾ｪ蜍募ｾｩ豢ｻ縺ｾ縺ｧ縺ｮ遘呈焚・・莉･荳九↑繧牙ｾｩ豢ｻ縺励↑縺・ｼ・
    
    // 貍泌・繝代Λ繝｡繝ｼ繧ｿ
    Vector4 blockColor_ = { 0.72f, 0.48f, 0.34f, 1.0f }; // 繝悶Ο繝・け縺ｮ濶ｲ・亥｣翫ｌ繧・☆縺・ｲｩ/繝ｬ繝ｳ繧ｬ隱ｿ・・
    float hitstopDuration_ = 0.06f;       // 遐ｴ螢頑凾縺ｮ繝偵ャ繝医せ繝医ャ繝玲凾髢難ｼ育ｧ抵ｼ・
    float cameraShakePower_ = 0.25f;      // 遐ｴ螢頑凾縺ｮ繧ｫ繝｡繝ｩ謠ｺ繧後・蠑ｷ縺・
    float cameraShakeDuration_ = 0.15f;   // 遐ｴ螢頑凾縺ｮ繧ｫ繝｡繝ｩ謠ｺ繧梧凾髢難ｼ育ｧ抵ｼ・

    // 蛻晄悄驟咲ｽｮ諠・ｱ
    Vector3 basePosition_{ 0.0f, 0.0f, 0.0f };
    Vector3 baseScale_{ 1.0f, 1.0f, 1.0f };

    // 遐ｴ迚・ヱ繝ｼ繝・ぅ繧ｯ繝ｫ繝ｪ繧ｹ繝・
    std::vector<Debris> debrisList_;
};
