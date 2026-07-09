#include "JumpBlock.h"
#include "../Player/Player2D.h"
#include "../MapChip2D.h"

void JumpBlock::Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) {
    gameObject_ = std::make_unique<GameObject>("JumpBlock");
    auto* tc = gameObject_->AddComponent<TransformComponent>();
    auto* prc = gameObject_->AddComponent<PrimitiveRendererComponent>();

    prc->Initialize(device, boxPrimitive);
    
    // 繧ｸ繝｣繝ｳ繝怜床縺ｮ濶ｲ・壹が繝ｬ繝ｳ繧ｸ濶ｲ
    prc->GetMaterial().color = { 1.0f, 0.5f, 0.0f, 1.0f };
    
    tc->SetScale({ width, height, 1.0f });
    tc->SetPosition({ worldX, worldY, 0.0f });
    prc->GetMaterial().lightingType = 1; // 繝ｩ繧､繝・ぅ繝ｳ繧ｰ辟｡蜉ｹ蛹・
}

void JumpBlock::OnPlayerStand() {
    // 繝励Ξ繧､繝､繝ｼ縺御ｹ励▲縺滓凾縺ｫ蜃ｦ逅・＆繧後ｋ蜿ｯ閭ｽ諤ｧ縺後≠繧九′縲・
    // 遒ｺ螳溘↓蜃ｦ逅・☆繧九◆繧√↓ OnCollision 縺ｧ繧ょ愛螳壹ｒ陦後≧縺ｮ縺悟ｮ牙・縲・
}

void JumpBlock::OnCollision(Player2D* player) {
    if (!player) return;

    // 繧ｸ繝｣繝ｳ繝怜床縺ｮ AABB 繧貞叙蠕・
    AABB2D blockAABB = GetAABB();
    // 繝励Ξ繧､繝､繝ｼ縺ｮ AABB 繧貞叙蠕・
    AABB2D playerAABB = player->GetAABB();
    
    // 蜻ｨ蝗ｲ縺ｮ繝悶Ο繝・け迥ｶ豕√ｒ蜿門ｾ・
    bool hasRight  = map_->GetBlock(chipX_ + 1, chipY_) != nullptr;
    bool hasLeft   = map_->GetBlock(chipX_ - 1, chipY_) != nullptr;
    bool hasTop    = map_->GetBlock(chipX_, chipY_ + 1) != nullptr;
    bool hasBottom = map_->GetBlock(chipX_, chipY_ - 1) != nullptr;

    bool isFloating = (!hasRight && !hasLeft && !hasTop && !hasBottom);

    // 謗･蝨ｰ髱｢・医ヶ繝ｭ繝・け縺後￥縺｣縺､縺・※縺・ｋ髱｢・峨ｒ蜆ｪ蜈亥ｺｦ鬆・↓蛻､螳壹＠縲√・縺ｭ縺ｮ譁ｹ蜷代ｒ荳縺､縺ｫ邨槭ｋ
    bool activeTop = false;
    bool activeBottom = false;
    bool activeLeft = false;
    bool activeRight = false;

    if (hasBottom) {
        activeTop = true; // 荳九↓繝悶Ο繝・け縺後≠繧九↑繧我ｸ企擇縺ｧ霍ｳ縺ｭ繧・
    } else if (hasLeft) {
        activeRight = true; // 蟾ｦ縺ｫ繝悶Ο繝・け縺後≠繧九↑繧牙承髱｢縺ｧ霍ｳ縺ｭ繧・
    } else if (hasRight) {
        activeLeft = true; // 蜿ｳ縺ｫ繝悶Ο繝・け縺後≠繧九↑繧牙ｷｦ髱｢縺ｧ霍ｳ縺ｭ繧・
    } else if (hasTop) {
        activeBottom = true; // 荳翫↓繝悶Ο繝・け縺後≠繧九↑繧我ｸ矩擇縺ｧ霍ｳ縺ｭ繧・
    } else {
        activeTop = true; // 螳悟・縺ｫ豬ｮ縺・※縺・ｋ蝣ｴ蜷医・繝・ヵ繧ｩ繝ｫ繝医〒荳企擇縺ｧ霍ｳ縺ｭ繧・
    }

    // 蜷・擇縺ｨ縺ｮ霍晞屬繧定ｨ育ｮ暦ｼ・layer2D蛛ｴ縺ｧ繧√ｊ霎ｼ縺ｿ縺梧款縺玲綾縺輔ｌ縺ｦ縺・ｋ縺溘ａ縲∵磁隗ｦ髱｢縺ｯ霍晞屬縺後⊇縺ｼ0縺ｫ縺ｪ繧具ｼ・
    float distTop = std::abs(playerAABB.bottom - blockAABB.top);
    float distBottom = std::abs(playerAABB.top - blockAABB.bottom);
    float distLeft = std::abs(playerAABB.right - blockAABB.left);
    float distRight = std::abs(playerAABB.left - blockAABB.right);

    float minDist = (std::min)({ distTop, distBottom, distLeft, distRight });

    Vector3 vel = player->GetVelocity();
    const float threshold = 0.15f; // 謗･隗ｦ蛻､螳壹・菴呵｣・

    if (minDist == distTop && distTop < threshold && activeTop) {
        vel.y = jumpVelocityVertical_;
        player->SetVelocity(vel);
    } else if (minDist == distBottom && distBottom < threshold && activeBottom) {
        vel.y = -jumpVelocityVertical_;
        player->SetVelocity(vel);
    } else if (minDist == distLeft && distLeft < threshold && activeLeft) {
        player->SetExternalVelocityX(-jumpVelocityHorizontal_);
        vel.y = 5.0f; // 蟆代＠荳翫↓豬ｮ縺九○繧九％縺ｨ縺ｧ謗･蝨ｰ蛻､螳壹ｒ隗｣髯､縺励∵・諤ｧ縺後☆縺舌↓豸医＆繧後ｋ縺ｮ繧帝亟縺・
        player->SetVelocity(vel);
        player->SetIsOnGround(false);
    } else if (minDist == distRight && distRight < threshold && activeRight) {
        player->SetExternalVelocityX(jumpVelocityHorizontal_);
        vel.y = 5.0f; // 蟆代＠荳翫↓豬ｮ縺九○繧・
        player->SetVelocity(vel);
        player->SetIsOnGround(false);
    }
}

void JumpBlock::SetProperties(const nlohmann::json& properties) {
    if (properties.contains("jumpVelocityVertical")) {
        jumpVelocityVertical_ = properties["jumpVelocityVertical"].get<float>();
    }
    if (properties.contains("jumpVelocityHorizontal")) {
        jumpVelocityHorizontal_ = properties["jumpVelocityHorizontal"].get<float>();
    }
    // 蜿､縺・ｽ｢蠑上・莠呈鋤諤ｧ邯ｭ謖・
    if (properties.contains("jumpVelocity")) {
        jumpVelocityVertical_ = properties["jumpVelocity"].get<float>();
        jumpVelocityHorizontal_ = properties["jumpVelocity"].get<float>();
    }
}
