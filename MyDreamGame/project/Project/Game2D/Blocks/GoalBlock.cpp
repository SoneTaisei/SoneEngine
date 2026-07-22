#include "GoalBlock.h"
#include "Game2D/Player/Player2D.h"

void GoalBlock::Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) {
    gameObject_ = std::make_unique<GameObject>("GoalBlock");
    auto* tc = gameObject_->AddComponent<TransformComponent>();
    auto* prc = gameObject_->AddComponent<PrimitiveRendererComponent>();

    prc->Initialize(device, boxPrimitive);
    prc->GetMaterial().color = { 0.8f, 0.2f, 0.8f, 1.0f }; // ゴールは紫色
    tc->SetScale({ width, height, 1.0f });
    tc->SetPosition({ worldX, worldY, 0.0f });
    prc->GetMaterial().lightingType = 1; // ライチEング無効匁E
    SetupCollider();
}

void GoalBlock::OnCollision(Player2D* player) {
    if (player) {
        player->ReachGoal();
    }
}
