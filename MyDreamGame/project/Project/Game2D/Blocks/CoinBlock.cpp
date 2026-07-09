#include "CoinBlock.h"
#include "Game2D/Player/Player2D.h"
#include "Core/TimeManager.h"

void CoinBlock::Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) {
    gameObject_ = std::make_unique<GameObject>("CoinBlock");
    auto* tc = gameObject_->AddComponent<TransformComponent>();
    auto* prc = gameObject_->AddComponent<PrimitiveRendererComponent>();

    prc->Initialize(device, boxPrimitive);
    prc->GetMaterial().color = { 1.0f, 0.8f, 0.0f, 1.0f };
    tc->SetScale({ width * 0.5f, height * 0.5f, 0.5f });
    position_ = { worldX, worldY, 0.0f };
    tc->SetPosition(position_);
    prc->GetMaterial().lightingType = 1; // 繝ｩ繧､繝・ぅ繝ｳ繧ｰ辟｡蜉ｹ蛹・
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
        // 荳翫↓荳翫′繧区ｼ泌・
        position_.y += 5.0f * deltaTime;
        
        // 蝗櫁ｻ｢繧呈掠繧√ｋ
        rotationY_ += 10.0f * deltaTime;
        
        // 繝輔ぉ繝ｼ繝峨い繧ｦ繝・
        if (auto* prc = gameObject_->GetComponent<PrimitiveRendererComponent>()) {
            prc->GetMaterial().color.w = 1.0f - progress;
        }
    } else {
        rotationY_ += 2.0f * deltaTime;
    }
    
    if (gameObject_) {
        if (auto* tc = gameObject_->GetComponent<TransformComponent>()) {
            tc->SetPosition(position_);
            tc->SetRotation({ 0.0f, rotationY_, 0.0f });
        }
        gameObject_->Update();
    }
}

void CoinBlock::OnCollision(Player2D* player) {
    if (!isDestroyed_ && !isCollected_ && player) {
        player->AddScore(100);
        isCollected_ = true;
    }
}
