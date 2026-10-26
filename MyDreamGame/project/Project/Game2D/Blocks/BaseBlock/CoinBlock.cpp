#include "CoinBlock.h"
#include "Core/TimeManager.h"
#include "Game2D/Player/Player2D.h"
#ifdef USE_IMGUI
#include "Editor/EditorManager.h"
#endif
#include "Editor/Replay/ReplayManager.h"

void CoinBlock::Initialize(ID3D12Device *device, Primitive *boxPrimitive, float worldX, float worldY, float width, float height) {
    (void)device;
    (void)boxPrimitive;
    SetGameObject(std::make_unique<GameObject>("CoinBlock"));
    auto *tc = GetGameObject()->AddComponent<TransformComponent>();

    tc->SetScale({width * 0.5f, height * 0.5f, 0.5f});
    position_ = {worldX, worldY, 0.0f};
    initialPosition_ = position_;
    tc->SetPosition(position_);

    // コインはJSON指定のモデルを使用するためボックス(PrimitiveRendererComponent)は追加しない
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
            if (auto *mrc = GetGameObject() ? GetGameObject()->GetComponent<MeshRendererComponent>() : nullptr) {
                Vector4 c = mrc->GetMaterial().color;
                c.w = 0.0f;
                mrc->SetColor(c);
            }
            if (auto *prc = GetGameObject() ? GetGameObject()->GetComponent<PrimitiveRendererComponent>() : nullptr) {
                Vector4 c = prc->GetMaterial().color;
                c.w = 0.0f;
                prc->SetColor(c);
            }
            return;
        }
        // 上に上がる演出
        position_.y += 5.0f * animDeltaTime;

        // 回転を早める
        rotationY_ += 10.0f * animDeltaTime;

        // フェードアウト
        if (auto *mrc = GetGameObject()->GetComponent<MeshRendererComponent>()) {
            Vector4 c = mrc->GetMaterial().color;
            c.w = 1.0f - progress;
            mrc->SetColor(c);
        }
        if (auto *prc = GetGameObject()->GetComponent<PrimitiveRendererComponent>()) {
            Vector4 c = prc->GetMaterial().color;
            c.w = 1.0f - progress;
            prc->SetColor(c);
        }
    } else {
        rotationY_ += 2.0f * animDeltaTime;
    }

    if (GetGameObject()) {
        if (auto *tc = GetGameObject()->GetComponent<TransformComponent>()) {
            tc->SetPosition(position_);
            tc->SetRotation({0.0f, rotationY_, 0.0f});
        }
        GetGameObject()->Update();
    }
}

void CoinBlock::OnCollision(Player2D *player) {
    if (GetGameObject() && !isCollected_ && player) {
        player->AddScore(100);
        isCollected_ = true;
        // 獲得後は二重衝突を防ぐため、コライダーのレイヤーマスクを解除する
        if (auto *cc = GetGameObject()->GetComponent<ColliderComponent>()) {
            cc->SetLayerMask(0);
        }
    }
}

void CoinBlock::Reset() {
    BaseBlock::Reset();
    isCollected_ = false;
    collectTimer_ = 0.0f;
    rotationY_ = 0.0f;
    position_ = initialPosition_;
    SetDestroyed(false);

    if (GetGameObject()) {
        if (auto *tc = GetGameObject()->GetComponent<TransformComponent>()) {
            tc->SetPosition(position_);
            tc->SetRotation({0.0f, 0.0f, 0.0f});
        }
        if (auto *mrc = GetGameObject()->GetComponent<MeshRendererComponent>()) {
            Vector4 c = mrc->GetMaterial().color;
            c.w = 1.0f;
            mrc->SetColor(c);
        }
        if (auto *prc = GetGameObject()->GetComponent<PrimitiveRendererComponent>()) {
            Vector4 c = prc->GetMaterial().color;
            c.w = 1.0f;
            prc->SetColor(c);
        }
        if (auto *cc = GetGameObject()->GetComponent<ColliderComponent>()) {
            cc->SetLayerMask(kLayerBlock);
        }
    }
}
