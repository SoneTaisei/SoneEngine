#include "MovingBlock.h"
#include "Game2D/Player/Player2D.h"
#include "Core/TimeManager.h"
#include <cmath>
#ifdef USE_IMGUI
#include <imgui.h>
#include "Editor/EditorManager.h"
#endif

MovingBlock::MovingBlock(MapChip2D* map, int chipX, int chipY)
    : BaseBlock(map, chipX, chipY) {}

void MovingBlock::Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) {
    startX_ = worldX;
    startY_ = worldY;
    prevPosition_ = {worldX, worldY, 0.0f};
    deltaPosition_ = {0.0f, 0.0f, 0.0f};

    gameObject_ = std::make_unique<GameObject>();
    gameObject_->Initialize();
    gameObject_->SetName("MovingBlock");

    auto* transform = gameObject_->AddComponent<TransformComponent>();
    transform->SetPosition({worldX, worldY, 0.0f});
    transform->SetScale({width, height, 1.0f});

    auto* renderer = gameObject_->AddComponent<PrimitiveRendererComponent>();
    renderer->Initialize(device, boxPrimitive);
    renderer->GetMaterial().color = {0.8f, 0.5f, 0.1f, 1.0f}; // オレンジっぽい色
    renderer->GetMaterial().lightingType = 1;

    SetupCollider();
}

void MovingBlock::SetProperties(const nlohmann::json& properties) {
    if (properties.contains("moveAxis") && properties["moveAxis"].is_string()) {
        moveAxis_ = properties["moveAxis"];
    }
    if (properties.contains("moveRange") && properties["moveRange"].is_number()) {
        moveRange_ = properties["moveRange"];
    }
    if (properties.contains("moveSpeed") && properties["moveSpeed"].is_number()) {
        moveSpeed_ = properties["moveSpeed"];
    }
}

void MovingBlock::Update() {
    BaseBlock::Update();
    if (!gameObject_) return;

#ifdef USE_IMGUI
    // エディタモード中は動かない
    if (!EditorManager::IsPlaying()) return;
#endif

    float dt = TimeManager::GetInstance().GetDeltaTime();
    if (dt <= 0.0f) return;
    
    timer_ += dt;

    auto* tc = gameObject_->GetComponent<TransformComponent>();
    if (tc) {
        // 配置された初期座標を元にタイミング（位相）をずらす
        float phase = startX_ * 0.5f + startY_ * 0.5f;
        float offset = std::sin(timer_ * moveSpeed_ + phase) * moveRange_;
        Vector3 newPos = {startX_, startY_, 0.0f};

        if (moveAxis_ == "X" || moveAxis_ == "x") {
            newPos.x += offset;
        } else if (moveAxis_ == "Y" || moveAxis_ == "y") {
            newPos.y += offset;
        }

        deltaPosition_ = {newPos.x - prevPosition_.x, newPos.y - prevPosition_.y, 0.0f};
        currentVelocity_ = {deltaPosition_.x / dt, deltaPosition_.y / dt, 0.0f};
        prevPosition_ = newPos;

        tc->SetPosition(newPos);
    }
}

void MovingBlock::OnPlayerStand(Player2D* player) {
    // 物理エンジン側で velocity を加算するため、ここでの直接座標操作は行わない
    (void)player;
}

#ifdef USE_IMGUI
void MovingBlock::DrawImGui() {
    // 設定は MapEditorInspector 側で自動生成されるため、ここでは何もしません
}
#endif
