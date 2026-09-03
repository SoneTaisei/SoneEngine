#include "ThinPlatformBlock.h"

void ThinPlatformBlock::SetProperties(const nlohmann::json& properties) {
    if (properties.contains("thickness") && properties["thickness"].is_number()) {
        thickness_ = properties["thickness"];
        if (thickness_ < 0.05f) thickness_ = 0.05f;
        if (thickness_ > 0.5f) thickness_ = 0.5f;
    }
}

void ThinPlatformBlock::Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) {
    gameObject_ = std::make_unique<GameObject>("ThinPlatformBlock");
    auto* tc = gameObject_->AddComponent<TransformComponent>();
    auto* prc = gameObject_->AddComponent<PrimitiveRendererComponent>();

    prc->Initialize(device, boxPrimitive);
    // 木の板の色。プレイヤーの片方向床判定はチップの上端を使うので、板もチップ上端に貼り付ける
    prc->GetMaterial().color = { 0.62f, 0.42f, 0.22f, 1.0f };
    prc->GetMaterial().lightingType = 1;
    float t = height * thickness_;
    tc->SetScale({ width, t, 1.0f });
    tc->SetPosition({ worldX, worldY + height * 0.5f - t * 0.5f, 0.0f });
    SetupCollider();
}
