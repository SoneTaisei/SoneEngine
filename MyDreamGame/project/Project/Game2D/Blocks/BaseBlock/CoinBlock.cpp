#include "CoinBlock.h"
#include "Game2D/Player/Player2D.h"
#include "Core/TimeManager.h"
#ifdef USE_IMGUI
#include "Editor/EditorManager.h"
#endif
#include "Editor/Replay/ReplayManager.h"

void CoinBlock::Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) {
    gameObject_ = std::make_unique<GameObject>("CoinBlock");
    auto* tc = gameObject_->AddComponent<TransformComponent>();
    auto* prc = gameObject_->AddComponent<PrimitiveRendererComponent>();

    prc->Initialize(device, boxPrimitive);
    prc->GetMaterial().color = { 1.0f, 0.8f, 0.0f, 1.0f };
    tc->SetScale({ width * 0.5f, height * 0.5f, 0.5f });
    position_ = { worldX, worldY, 0.0f };
    tc->SetPosition(position_);
    prc->GetMaterial().lightingType = 1;
    SetupCollider();
}

void CoinBlock::Update() {
    float deltaTime = TimeManager::GetInstance().GetDeltaTime();
    
    bool isPlayingOrReplaying = false;
#ifdef USE_IMGUI
    if (EditorManager::IsPlaying()) {
        isPlayingOrReplaying = true;
    }
#else
    isPlayingOrReplaying = true;
#endif
    if (ReplayManager::GetInstance()->IsPlaying()) {
        isPlayingOrReplaying = true;
    }

    bool isAnimActive = isPlayingOrReplaying && !ReplayManager::GetInstance()->IsPaused();
    float animDeltaTime = isAnimActive ? deltaTime : 0.0f;

    if (isCollected_) {
        collectTimer_ += animDeltaTime;
        float progress = collectTimer_ / collectDuration_;
        if (progress >= 1.0f) {
            Destroy();
            return;
        }
        // 上に上がる演出
        position_.y += 5.0f * animDeltaTime;
        
        // 回転を早める
        rotationY_ += 10.0f * animDeltaTime;
        
        // フェードアウト
        if (auto* prc = gameObject_->GetComponent<PrimitiveRendererComponent>()) {
            prc->GetMaterial().color.w = 1.0f - progress;
        }
    } else {
        rotationY_ += 2.0f * animDeltaTime;
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
