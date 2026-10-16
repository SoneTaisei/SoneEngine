#include "OneWayBlock.h"

void OneWayBlock::Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) {
    SetGameObject(std::make_unique<GameObject>("OneWayBlock"));
    auto *tc = GetGameObject()->AddComponent<TransformComponent>();
    auto *prc = GetGameObject()->AddComponent<PrimitiveRendererComponent>();

    prc->Initialize(device, boxPrimitive);
    prc->SetColor({0.4f, 0.8f, 0.8f, 1.0f});
    tc->SetScale({ width, height * 0.3f, 1.0f });
    tc->SetPosition({ worldX, worldY + height * 0.35f, 0.0f });
    prc->SetLightingType(1); // ライチEング無効匁E
    SetupCollider();
}
