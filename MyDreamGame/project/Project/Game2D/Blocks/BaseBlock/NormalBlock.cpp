#include "NormalBlock.h"

void NormalBlock::Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) {
    SetGameObject(std::make_unique<GameObject>("NormalBlock"));
    auto *tc = GetGameObject()->AddComponent<TransformComponent>();
    auto *prc = GetGameObject()->AddComponent<PrimitiveRendererComponent>();
    
    prc->Initialize(device, boxPrimitive);
    if (GetChipY() <= 1) {
        // 地面：茶色
        prc->SetColor({ 0.55f, 0.35f, 0.17f, 1.0f });
    } else {
        // 壁：緑色
        prc->SetColor({ 0.4f, 0.8f, 0.4f, 1.0f });
    }
    tc->SetScale({ width, height, 1.0f });
    tc->SetPosition({ worldX, worldY, 0.0f });
    prc->SetLightingType(1); // ライティング有効化
    SetupCollider();
}
