#include "NormalBlock.h"

void NormalBlock::Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) {
    gameObject_ = std::make_unique<GameObject>("NormalBlock");
    auto* tc = gameObject_->AddComponent<TransformComponent>();
    auto* prc = gameObject_->AddComponent<PrimitiveRendererComponent>();
    
    prc->Initialize(device, boxPrimitive);
    if (chipY_ <= 1) {
        // 蝨ｰ髱｢・夊幻濶ｲ
        prc->GetMaterial().color = { 0.55f, 0.35f, 0.17f, 1.0f };
    } else {
        // 螢・ｼ夂ｷ題牡
        prc->GetMaterial().color = { 0.4f, 0.8f, 0.4f, 1.0f };
    }
    tc->SetScale({ width, height, 1.0f });
    tc->SetPosition({ worldX, worldY, 0.0f });
    prc->GetMaterial().lightingType = 1; // ライティング有効化
}
