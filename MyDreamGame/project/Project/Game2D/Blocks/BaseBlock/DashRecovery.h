#pragma once
#include "Game2D/Blocks/BaseBlock.h"
#include <memory>
#include "Effect/GPUParticle/GPUParticleSystem.h"

/// <summary>
/// DashRecovery - 繝繝・す繝･蝗槫ｾｩ繝悶Ο繝・け/繧｢繧､繝・Β
/// 繝励Ξ繧､繝､繝ｼ縺梧磁隗ｦ縺吶ｋ縺ｨ繝繝・す繝･蝗樊焚繧貞叉蠎ｧ縺ｫ蝗槫ｾｩ縺励；PU繝代・繝・ぅ繧ｯ繝ｫ繧ｨ繝輔ぉ繧ｯ繝医ｒ蜀咲函縺励∪縺吶・
/// </summary>
class DashRecovery : public BaseBlock {
public:
    using BaseBlock::BaseBlock;
    ~DashRecovery() override;

    // 蛻晄悄蛹門・逅・ｼ医Δ繝・Ν繝ｻ繝槭ユ繝ｪ繧｢繝ｫ繝ｻ繧ｳ繝ｩ繧､繝繝ｼ繝ｻGPU繝代・繝・ぅ繧ｯ繝ｫ縺ｮ繧ｻ繝・ヨ繧｢繝・・・・
    void Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) override;

    // 豈弱ヵ繝ｬ繝ｼ繝縺ｮ譖ｴ譁ｰ蜃ｦ逅・ｼ域ｵｮ驕翫い繝九Γ繝ｼ繧ｷ繝ｧ繝ｳ繝ｻ繝ｪ繧ｹ繝昴・繝ｳ邂｡逅・・GPU繝代・繝・ぅ繧ｯ繝ｫ譖ｴ譁ｰ・・
    void Update() override;

    // 謠冗判蜃ｦ逅・ｼ域悽菴薙♀繧医・GPU繝代・繝・ぅ繧ｯ繝ｫ謠冗判・・
    void Draw() override;

    // 蠖薙◆繧雁愛螳壹・諤ｧ雉ｪ・医ョ繝輔か繝ｫ繝医〒縺吶ｊ謚懊￠繧｢繧､繝・Β縲∬ｨｭ螳壹〒雜ｳ蝣ｴ蛹門庄閭ｽ・・
    bool IsSolid() const override { return isSolid_ && isActive_; }
    bool IsOneWay() const override { return false; }

    // 繝励Ξ繧､繝､繝ｼ縺梧磁隗ｦ縺励◆迸ｬ髢薙・蜃ｦ逅・ｼ医ム繝・す繝･蝗槫ｾｩ繝ｻ繧ｨ繝輔ぉ繧ｯ繝育匱蜍包ｼ・
    void OnCollision(Player2D* player) override;

    // 繝励Ξ繧､繝､繝ｼ縺後ヶ繝ｭ繝・け縺ｮ荳翫↓荵励▲縺ｦ縺・ｋ譎ゅ・蜃ｦ逅・
    void OnPlayerStand() override;

    // 繝励Ξ繧､繝､繝ｼ縺梧ｨｪ繧・ｸ九°繧芽ｧｦ繧後◆譎ゅ・蜃ｦ逅・
    void OnPlayerTouch() override;

    // JSON繝励Ο繝代ユ繧｣縺ｮ隱ｭ縺ｿ霎ｼ縺ｿ・医お繝・ぅ繧ｿ縺ｮ繧､繝ｳ繧ｹ繝壹け繧ｿ繝ｼ縺九ｉ縺ｮ繝代Λ繝｡繝ｼ繧ｿ險ｭ螳夲ｼ・
    void SetProperties(const nlohmann::json& properties) override;

    // 繧ｹ繝・・繧ｸ蜀埼幕譎ゅ・繝ｪ繝医Λ繧､譎ゅ・迥ｶ諷九Μ繧ｻ繝・ヨ蜃ｦ逅・
    void Reset() override;

#ifdef USE_IMGUI
    // ImGui縺ｫ繧医ｋ繝ｪ繧｢繝ｫ繧ｿ繧､繝繝代Λ繝｡繝ｼ繧ｿ隱ｿ謨ｴ繝ｻ繝・ヰ繝・げ逕ｨUI髢｢謨ｰ
    void DrawImGui() override;
#endif

    // 蝗槫ｾｩ繝ｻ蜿門ｾ怜・逅・ｼ・ashRecaveryEffect縺ｮ蜀咲函蜷ｫ繧・・
    void Collect(Player2D* player);

    // 繝ｪ繧ｹ繝昴・繝ｳ・亥ｾｩ豢ｻ・牙・逅・
    void Respawn();

private:
    // 驟咲ｽｮ繝ｻ螟冶ｦｳ繝代Λ繝｡繝ｼ繧ｿ
    Vector3 basePosition_{ 0.0f, 0.0f, 0.0f };
    Vector3 baseScale_{ 1.0f, 1.0f, 1.0f };
    Vector4 color_{ 0.35f, 0.85f, 1.0f, 1.0f }; // 魄ｮ繧・°縺ｪ繧ｷ繧｢繝ｳ繧ｯ繝ｪ繧ｹ繧ｿ繝ｫ濶ｲ
    float inactiveAlpha_ = 0.25f;               // 繧ｯ繝ｼ繝ｫ繝繧ｦ繝ｳ荳ｭ縺ｮ繧ｴ繝ｼ繧ｹ繝磯乗・蠎ｦ

    // 繧｢繝九Γ繝ｼ繧ｷ繝ｧ繝ｳ逕ｨ
    float hoverTimer_ = 0.0f;
    float hoverSpeed_ = 3.0f;
    float hoverAmplitude_ = 0.12f;
    float rotationSpeed_ = 1.8f;
    float respawnAnimTimer_ = 0.0f;
    const float respawnAnimDuration_ = 0.35f;

    // 繧ｲ繝ｼ繝繝励Ξ繧､繝代Λ繝｡繝ｼ繧ｿ
    bool isSolid_ = false;              // 雜ｳ蝣ｴ縺ｨ縺励※荵励ｌ繧九ｈ縺・↓縺吶ｋ縺具ｼ・alse縺ｧ縺吶ｊ謚懊￠繧｢繧､繝・Β・・
    bool isActive_ = true;              // 迴ｾ蝨ｨ蜿門ｾ怜庄閭ｽ縺・
    float respawnTime_ = 2.5f;          // 蜿門ｾ怜ｾ後・蠕ｩ豢ｻ譎る俣・育ｧ抵ｼ・
    float respawnTimer_ = 0.0f;         // 繧ｯ繝ｼ繝ｫ繝繧ｦ繝ｳ繧ｿ繧､繝槭・
    float hitstopDuration_ = 0.03f;     // 蜿門ｾ玲凾縺ｮ繝偵ャ繝医せ繝医ャ繝玲凾髢・
    float cameraShakePower_ = 0.15f;    // 蜿門ｾ玲凾縺ｮ蠕ｮ蟆上↑繧ｫ繝｡繝ｩ繧ｷ繧ｧ繧､繧ｯ

    // 繧ｨ繝輔ぉ繧ｯ繝郁ｪｿ謨ｴ繝代Λ繝｡繝ｼ繧ｿ
    bool useBurstTrigger_ = false;      // 繝舌・繧ｹ繝亥ｼｷ蛻ｶ蜈ｨ謾ｾ蜃ｺ繝｢繝ｼ繝会ｼ・FF: 繧ｨ繝・ぅ繧ｿ騾壹ｊ縺ｮ閾ｪ辟ｶ縺ｪ逋ｺ逕滂ｼ・

    // GPU繝代・繝・ぅ繧ｯ繝ｫ繧ｨ繝輔ぉ繧ｯ繝・(DashRecaveryEffect)
    std::unique_ptr<GPUParticleSystem> gpuParticleSystem_;

    // 繧ｰ繝ｭ繝ｼ繝舌Ν騾｣邯壼叙蠕鈴亟豁｢繧ｬ繝ｼ繝・& 繝・ヰ繝・げ邨ｱ險・
    static float sGlobalCollectCooldown_;
    static int sTotalCollectCount_;
};
