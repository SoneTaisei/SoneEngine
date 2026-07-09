#include "OneWayBlock.h"

void OneWayBlock::Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) {
    gameObject_ = std::make_unique<GameObject>("OneWayBlock");
    auto* tc = gameObject_->AddComponent<TransformComponent>();
    auto* prc = gameObject_->AddComponent<PrimitiveRendererComponent>();

    prc->Initialize(device, boxPrimitive);
    prc->GetMaterial().color = { 0.4f, 0.8f, 0.8f, 1.0f };
    tc->SetScale({ width, height * 0.3f, 1.0f });
    tc->SetPosition({ worldX, worldY + height * 0.35f, 0.0f });
    prc->GetMaterial().lightingType = 1; // ライチE��ング無効匁E
}
