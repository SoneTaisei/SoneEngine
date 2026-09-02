#include "FragileBlock.h"
#include "Game2D/Player/Player2D.h"
#include "Core/TimeManager.h"
#include <cmath>
#ifdef USE_IMGUI
#include <imgui.h>
#endif

// ランダムな揺れ用
#include <random>

FragileBlock::FragileBlock(MapChip2D* map, int chipX, int chipY)
    : BaseBlock(map, chipX, chipY) {}

void FragileBlock::Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) {
    startX_ = worldX;
    startY_ = worldY;

    gameObject_ = std::make_unique<GameObject>();
    gameObject_->Initialize();
    gameObject_->SetName("FragileBlock");

    auto* transform = gameObject_->AddComponent<TransformComponent>();
    transform->SetPosition({worldX, worldY, 0.0f});
    transform->SetScale({width, height, 1.0f});

    auto* renderer = gameObject_->AddComponent<PrimitiveRendererComponent>();
    renderer->Initialize(device, boxPrimitive);
    renderer->GetMaterial().color = {0.4f, 0.4f, 0.4f, 1.0f}; // ひび割れた石のようなグレーに変更
    renderer->GetMaterial().lightingType = 1;

    SetupCollider();
}

void FragileBlock::SetProperties(const nlohmann::json& properties) {
    if (properties.contains("breakWeight") && properties["breakWeight"].is_number()) {
        breakWeight_ = properties["breakWeight"];
    }
    if (properties.contains("breakDuration") && properties["breakDuration"].is_number()) {
        breakDuration_ = properties["breakDuration"];
    }
}

void FragileBlock::Update() {
    BaseBlock::Update();
    if (isDestroyed_ || !gameObject_) return;

    if (isBreaking_) {
        float dt = TimeManager::GetInstance().GetDeltaTime();
        breakTimer_ += dt;

        auto* tc = gameObject_->GetComponent<TransformComponent>();
        auto* renderer = gameObject_->GetComponent<PrimitiveRendererComponent>();

        if (breakTimer_ >= breakDuration_) {
            // 崩壊
            isDestroyed_ = true;
        } else {
            // 揺れる演出
            float progress = breakTimer_ / breakDuration_;
            
            // ランダムな揺れ幅（時間が経つほど激しくなる）
            static std::mt19937 randEngine(std::random_device{}());
            std::uniform_real_distribution<float> dist(-0.1f * progress, 0.1f * progress);
            
            Vector3 newPos = {startX_ + dist(randEngine), startY_ + dist(randEngine), 0.0f};
            if (tc) tc->SetPosition(newPos);

            // 色を徐々に赤くする
            if (renderer) {
                Vector4 startColor = {0.8f, 0.7f, 0.5f, 1.0f};
                Vector4 endColor = {1.0f, 0.2f, 0.2f, 1.0f};
                Vector4 currentColor = {
                    startColor.x + (endColor.x - startColor.x) * progress,
                    startColor.y + (endColor.y - startColor.y) * progress,
                    startColor.z + (endColor.z - startColor.z) * progress,
                    1.0f
                };
                renderer->GetMaterial().color = currentColor;
            }
        }
    }
}

void FragileBlock::OnPlayerStand(Player2D* player) {
    if (!isBreaking_ && player) {
        if (player->GetChainLength() >= breakWeight_) {
            isBreaking_ = true;
            breakTimer_ = 0.0f;
        }
    }
}

void FragileBlock::Reset() {
    isDestroyed_ = false;
    isBreaking_ = false;
    breakTimer_ = 0.0f;

    if (gameObject_) {
        auto* tc = gameObject_->GetComponent<TransformComponent>();
        if (tc) tc->SetPosition({startX_, startY_, 0.0f});
        
        auto* renderer = gameObject_->GetComponent<PrimitiveRendererComponent>();
        if (renderer) renderer->GetMaterial().color = {0.4f, 0.4f, 0.4f, 1.0f};
    }
}

#ifdef USE_IMGUI
void FragileBlock::DrawImGui() {
    // 設定は MapEditorInspector 側で自動生成されるため、ここでは何もしません
}
#endif
