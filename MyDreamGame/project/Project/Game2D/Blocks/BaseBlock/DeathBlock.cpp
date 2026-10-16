#include "DeathBlock.h"
#include "Game2D/Player/Player2D.h"

void DeathBlock::Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) {
    SetGameObject(std::make_unique<GameObject>("DeathBlock"));
    auto *tc = GetGameObject()->AddComponent<TransformComponent>();
    auto *prc = GetGameObject()->AddComponent<PrimitiveRendererComponent>();
    
    prc->Initialize(device, boxPrimitive);
    prc->SetColor({1.0f, 0.2f, 0.2f, 1.0f});
    tc->SetScale({ width, height, 1.0f });
    tc->SetPosition({ worldX, worldY, 0.0f });
    prc->SetLightingType(1); // ライチEング無効匁E
    SetupCollider();
}

void DeathBlock::OnCollision(Player2D* player) {
    if (player) {
        player->Kill();
    }
}
