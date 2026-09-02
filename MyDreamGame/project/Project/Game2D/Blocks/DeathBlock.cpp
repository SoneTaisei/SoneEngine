#include "DeathBlock.h"
#include "Game2D/Player/Player2D.h"

void DeathBlock::Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) {
    gameObject_ = std::make_unique<GameObject>("DeathBlock");
    auto* tc = gameObject_->AddComponent<TransformComponent>();
    auto* prc = gameObject_->AddComponent<PrimitiveRendererComponent>();
    
    prc->Initialize(device, boxPrimitive);
    prc->GetMaterial().color = { 1.0f, 0.2f, 0.2f, 1.0f };
    tc->SetScale({ width, height, 1.0f });
    tc->SetPosition({ worldX, worldY, 0.0f });
    prc->GetMaterial().lightingType = 1; // ライチEング無効匁E
    SetupCollider();
}

void DeathBlock::OnCollision(Player2D* player) {
    if (player) {
        player->Kill();
    }
}

void DeathBlock::OnPlayerStand(Player2D* player) {
    if (player) {
        player->Kill();
    }
}

void DeathBlock::OnPlayerTouch(Player2D* player) {
    if (player) {
        player->Kill();
    }
}
