#include "GoalBlock.h"
#include "Game2D/Player/Player2D.h"
#include "Core/TimeManager.h"
#include "Editor/Replay/ReplayManager.h"
#ifdef USE_IMGUI
#include "Editor/EditorManager.h"
#include <imgui.h>
#endif
#include <cmath>

void GoalBlock::Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) {
    gameObject_ = std::make_unique<GameObject>("GoalBlock");
    auto* tc = gameObject_->AddComponent<TransformComponent>();
    auto* prc = gameObject_->AddComponent<PrimitiveRendererComponent>();

    prc->Initialize(device, boxPrimitive);
    prc->GetMaterial().color = { 0.8f, 0.2f, 0.8f, 1.0f }; // ゴールは紫色
    tc->SetScale({ width, height, 1.0f });
    basePosition_ = { worldX, worldY, 0.0f };
    tc->SetPosition(basePosition_);
    prc->GetMaterial().lightingType = 1;
    SetupCollider();
}

void GoalBlock::Update() {
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

    // 上下浮遊の更新
    if (animDeltaTime > 0.0f) {
        floatTimer_ += animDeltaTime * floatSpeed_;
    }

    float offsetY = std::sin(floatTimer_) * floatAmplitude_;

    if (gameObject_) {
        if (auto* tc = gameObject_->GetComponent<TransformComponent>()) {
            tc->SetPosition({ basePosition_.x, basePosition_.y + offsetY, basePosition_.z });
        }
        gameObject_->Update();
    }
}

void GoalBlock::Reset() {
    floatTimer_ = 0.0f;
    if (gameObject_) {
        if (auto* tc = gameObject_->GetComponent<TransformComponent>()) {
            tc->SetPosition(basePosition_);
        }
    }
}

void GoalBlock::OnCollision(Player2D* player) {
    if (player) {
        player->ReachGoal();
    }
}

void GoalBlock::SetProperties(const nlohmann::json& properties) {
    if (properties.contains("floatAmplitude")) {
        floatAmplitude_ = properties["floatAmplitude"].get<float>();
    }
    if (properties.contains("floatSpeed")) {
        floatSpeed_ = properties["floatSpeed"].get<float>();
    }
}

#ifdef USE_IMGUI
void GoalBlock::DrawImGui() {
    ImGui::Text("ゴール設定 (Goal Settings)");
    ImGui::DragFloat("浮遊の振幅", &floatAmplitude_, 0.01f, 0.0f, 5.0f);
    ImGui::DragFloat("浮遊の速度", &floatSpeed_, 0.05f, 0.0f, 20.0f);
}
#endif

