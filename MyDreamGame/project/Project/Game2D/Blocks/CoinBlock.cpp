#include "CoinBlock.h"
#include "Game2D/Player2D.h"
#include "Core/TimeManager.h"

void CoinBlock::Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float size) {
    primitiveObj_ = std::make_unique<PrimitiveObject>();
    primitiveObj_->Initialize(device, boxPrimitive);
    primitiveObj_->GetMaterial().color = { 1.0f, 0.8f, 0.0f, 1.0f };
    primitiveObj_->SetScale({ size * 0.5f, size * 0.5f, size * 0.5f });
    primitiveObj_->SetTranslation({ worldX, worldY, 0.0f });
    primitiveObj_->GetMaterial().lightingType = 0; // ライティング無効化
}

void CoinBlock::Update() {
    float deltaTime = TimeManager::GetInstance().GetDeltaTime();
    rotationY_ += 2.0f * deltaTime;
    
    if (primitiveObj_) {
        primitiveObj_->SetRotation({ 0.0f, rotationY_, 0.0f });
        primitiveObj_->Update();
    }
}

void CoinBlock::OnCollision(Player2D* player) {
    if (!isDestroyed_ && player) {
        player->AddScore(100);
        Destroy();
    }
}
