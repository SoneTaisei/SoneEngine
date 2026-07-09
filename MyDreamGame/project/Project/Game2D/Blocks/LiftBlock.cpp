#include "LiftBlock.h"
#include "../MapChip2D.h"
#include <cmath>

namespace {
    bool IsRailOrLift(MapChip2D* map, int x, int y) {
        auto type = map->GetChipType(x, y);
        if (type == MapChip2D::ChipType::kRail || type == MapChip2D::ChipType::kLift) return true;
        
        int typeId = static_cast<int>(type);
        if (typeId >= 100) {
            for (const auto& def : map->GetCustomPalette()) {
                if (def.id == typeId && (def.type == "RailBlock" || def.type == "LiftBlock")) return true;
            }
        } else if (typeId >= 1 && typeId <= 9) {
            for (const auto& def : map->GetTemplatePalette()) {
                if (def.id == typeId && (def.type == "RailBlock" || def.type == "LiftBlock")) return true;
            }
        }
        return false;
    }
}

void LiftBlock::SetProperties(const nlohmann::json& properties) {
    if (properties.contains("speedForward")) {
        speedForward_ = properties["speedForward"].get<float>();
    }
    if (properties.contains("speedBackward")) {
        speedBackward_ = properties["speedBackward"].get<float>();
    }
    if (properties.contains("waitTime")) {
        waitTime_ = properties["waitTime"].get<float>();
    }
    if (properties.contains("acceleration")) {
        acceleration_ = properties["acceleration"].get<float>();
    }
}

void LiftBlock::Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) {
    gameObject_ = std::make_unique<GameObject>("LiftBlock");
    auto* tc = gameObject_->AddComponent<TransformComponent>();
    auto* prc = gameObject_->AddComponent<PrimitiveRendererComponent>();

    prc->Initialize(device, boxPrimitive);
    
    // リフトの色�E�例：�Eるい黁E��めE��レンジ�E�E
    prc->GetMaterial().color = { 0.9f, 0.6f, 0.1f, 1.0f };
    tc->SetScale({ width, height, 1.0f });
    tc->SetPosition({ worldX, worldY, 0.0f });
    prc->GetMaterial().lightingType = 1;

    // レールの篁E��を探索して移動篁E��を決定すめE
    int spanWidth = static_cast<int>(std::round(width / map_->GetChipSize()));
    int spanHeight = static_cast<int>(std::round(height / map_->GetChipSize()));

    int minX = chipX_, maxX = chipX_ + spanWidth - 1;
    for (int cy = chipY_; cy < chipY_ + spanHeight; ++cy) {
        int tempMinX = chipX_;
        while (tempMinX - 1 >= 0 && IsRailOrLift(map_, tempMinX - 1, cy)) {
            tempMinX--;
        }
        if (tempMinX < minX) minX = tempMinX;

        int tempMaxX = chipX_ + spanWidth - 1;
        while (tempMaxX + 1 < map_->GetWidth() && IsRailOrLift(map_, tempMaxX + 1, cy)) {
            tempMaxX++;
        }
        if (tempMaxX > maxX) maxX = tempMaxX;
    }

    int minY = chipY_, maxY = chipY_ + spanHeight - 1;
    for (int cx = chipX_; cx < chipX_ + spanWidth; ++cx) {
        int tempMinY = chipY_;
        while (tempMinY - 1 >= 0 && IsRailOrLift(map_, cx, tempMinY - 1)) {
            tempMinY--;
        }
        if (tempMinY < minY) minY = tempMinY;

        int tempMaxY = chipY_ + spanHeight - 1;
        while (tempMaxY + 1 < map_->GetHeight() && IsRailOrLift(map_, cx, tempMaxY + 1)) {
            tempMaxY++;
        }
        if (tempMaxY > maxY) maxY = tempMaxY;
    }

    bool hasHorizontalRail = (minX < chipX_) || (maxX > chipX_ + spanWidth - 1);
    bool hasVerticalRail = (minY < chipY_) || (maxY > chipY_ + spanHeight - 1);

    if (hasHorizontalRail) {
        direction_ = { 1.0f, 0.0f, 0.0f }; // 水平移勁E
        // 実際の移動�E中忁E��置からなので調整
        minRailWorldX_ = map_->ChipToWorldX(minX) + width * 0.5f;
        maxRailWorldX_ = map_->ChipToWorldX(maxX - spanWidth + 1) + width * 0.5f;

        if (std::abs(worldX - minRailWorldX_) <= std::abs(worldX - maxRailWorldX_)) {
            startPos_ = { minRailWorldX_, worldY, 0.0f };
            endPos_ = { maxRailWorldX_, worldY, 0.0f };
        } else {
            startPos_ = { maxRailWorldX_, worldY, 0.0f };
            endPos_ = { minRailWorldX_, worldY, 0.0f };
        }
    } else if (hasVerticalRail) {
        direction_ = { 0.0f, 1.0f, 0.0f }; // 垂直移勁E
        minRailWorldY_ = map_->ChipToWorldY(minY) + height * 0.5f;
        maxRailWorldY_ = map_->ChipToWorldY(maxY - spanHeight + 1) + height * 0.5f;

        if (std::abs(worldY - minRailWorldY_) <= std::abs(worldY - maxRailWorldY_)) {
            startPos_ = { worldX, minRailWorldY_, 0.0f };
            endPos_ = { worldX, maxRailWorldY_, 0.0f };
        } else {
            startPos_ = { worldX, maxRailWorldY_, 0.0f };
            endPos_ = { worldX, minRailWorldY_, 0.0f };
        }
    } else {
        direction_ = { 0.0f, 0.0f, 0.0f }; // レールがなぁE��合�E動かなぁE
        startPos_ = { worldX, worldY, 0.0f };
        endPos_ = { worldX, worldY, 0.0f };
    }

    if (auto* tc = gameObject_->GetComponent<TransformComponent>()) {
        tc->SetPosition(startPos_);
    }

    velocity_ = { 0.0f, 0.0f, 0.0f };
    currentT_ = 0.0f;
    state_ = LiftState::IdleAtStart;
    waitTimer_ = 0.0f;
    isPlayerStandingThisFrame_ = false;
    SetupCollider();
}

void LiftBlock::Update() {
    float deltaTime = TimeManager::GetInstance().GetDeltaTime();
    if (deltaTime <= 0.0f) return;

    Vector3 prevPos = {0.0f, 0.0f, 0.0f};
    if (auto* tc = gameObject_->GetComponent<TransformComponent>()) {
        prevPos = tc->GetPosition();
    }

    // 距離を計箁E
    float distance = 0.0f;
    if (direction_.x != 0.0f) {
        distance = std::abs(endPos_.x - startPos_.x);
    } else if (direction_.y != 0.0f) {
        distance = std::abs(endPos_.y - startPos_.y);
    }

    if (distance <= 0.0f) return;

    switch (state_) {
        case LiftState::IdleAtStart:
            currentT_ = 0.0f;
            waitTimer_ = 0.0f;
            if (isPlayerStandingThisFrame_) {
                state_ = LiftState::MovingForward;
            }
            break;
            
        case LiftState::MovingForward: {
            float tRate = speedForward_ / distance;
            currentT_ += tRate * deltaTime;
            if (currentT_ >= 1.0f) {
                currentT_ = 1.0f;
                state_ = LiftState::WaitingAtEnd;
                waitTimer_ = 0.0f;
            }
            break;
        }
            
        case LiftState::WaitingAtEnd:
            currentT_ = 1.0f;
            waitTimer_ += deltaTime;
            if (waitTimer_ >= waitTime_) { // waitTime_征E��E
                state_ = LiftState::MovingBackward;
            }
            break;
            
        case LiftState::MovingBackward: {
            float tRate = speedBackward_ / distance;
            currentT_ -= tRate * deltaTime;
            if (currentT_ <= 0.0f) {
                currentT_ = 0.0f;
                state_ = LiftState::IdleAtStart;
            }
            break;
        }
    }

    float progress = 0.0f;
    if (state_ == LiftState::MovingForward) {
        // 加速度パラメータを使用したイージング (Power Ease In)
        progress = std::pow(currentT_, acceleration_);
    } else {
        // 復路および征E��時はLinear
        progress = currentT_;
    }

    Vector3 newPos = {
        startPos_.x + (endPos_.x - startPos_.x) * progress,
        startPos_.y + (endPos_.y - startPos_.y) * progress,
        startPos_.z + (endPos_.z - startPos_.z) * progress
    };

    // プレイヤーに渡す用の速度を更新
    velocity_.x = (newPos.x - prevPos.x) / deltaTime;
    velocity_.y = (newPos.y - prevPos.y) / deltaTime;

    if (auto* tc = gameObject_->GetComponent<TransformComponent>()) {
        tc->SetPosition(newPos);
    }
    if (gameObject_) {
        gameObject_->Update();
    }

    // 1フレームのフラグをリセチE��
    isPlayerStandingThisFrame_ = false;
}
