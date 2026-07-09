#include "RailBlock.h"

void RailBlock::Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) {
    gameObject_ = std::make_unique<GameObject>("RailBlock");
    auto* tc = gameObject_->AddComponent<TransformComponent>();
    auto* prc = gameObject_->AddComponent<PrimitiveRendererComponent>();

    prc->Initialize(device, boxPrimitive);
    
    // レール�E�少し暗い灰色
    prc->GetMaterial().color = { 0.4f, 0.4f, 0.4f, 1.0f };
    
    // 背景にあるように見せるため、少し奥に配置
    tc->SetPosition({ worldX, worldY, 0.5f });
    
    // レールとしての見た目を作るために少し細くすめE
    tc->SetScale({ width * 0.2f, height * 0.2f, 1.0f });
    
    prc->GetMaterial().lightingType = 1; // ライチE��ング無効匁E
}
