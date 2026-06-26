#include "CoinBlock.h"
#include "Game2D/Player2D.h"
#include "Core/TimeManager.h"

void CoinBlock::Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) {
    primitiveObj_ = std::make_unique<PrimitiveObject>();
    primitiveObj_->Initialize(device, boxPrimitive);
    primitiveObj_->GetMaterial().color = { 1.0f, 0.8f, 0.0f, 1.0f };
    primitiveObj_->SetScale({ width * 0.5f, height * 0.5f, 0.5f });
    position_ = { worldX, worldY, 0.0f };
    primitiveObj_->SetTranslation(position_);
    primitiveObj_->GetMaterial().lightingType = 0; // ライティング無効化
}

void CoinBlock::Update() {
    float deltaTime = TimeManager::GetInstance().GetDeltaTime();
    
    if (isCollected_) {
        collectTimer_ += deltaTime;
        float progress = collectTimer_ / collectDuration_;
        if (progress >= 1.0f) {
            Destroy();
            return;
        }
        // 上に上がる演出
        position_.y += 5.0f * deltaTime;
        primitiveObj_->SetTranslation(position_);
        
        // 回転を早める
        rotationY_ += 10.0f * deltaTime;
        
        // フェードアウト
        primitiveObj_->GetMaterial().color.w = 1.0f - progress;
    } else {
        rotationY_ += 2.0f * deltaTime;
    }
    
    if (primitiveObj_) {
        primitiveObj_->SetRotation({ 0.0f, rotationY_, 0.0f });
        primitiveObj_->Update();
    }
}

void CoinBlock::OnCollision(Player2D* player) {
    if (!isDestroyed_ && !isCollected_ && player) {
        player->AddScore(100);
        isCollected_ = true;
    }
}
