#include "LiftBlock.h"
#include "../MapChip2D.h"

void LiftBlock::Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) {
    primitiveObj_ = std::make_unique<PrimitiveObject>();
    primitiveObj_->Initialize(device, boxPrimitive);
    
    // リフトの色（例：明るい黄色やオレンジ）
    primitiveObj_->GetMaterial().color = { 0.9f, 0.6f, 0.1f, 1.0f };
    primitiveObj_->SetScale({ width, height, 1.0f });
    primitiveObj_->SetTranslation({ worldX, worldY, 0.0f });
    primitiveObj_->GetMaterial().lightingType = 0;

    // レールの範囲を探索して移動範囲を決定する
    int minX = chipX_, maxX = chipX_;
    while (minX - 1 >= 0 && (map_->GetChipType(minX - 1, chipY_) == MapChip2D::ChipType::kRail || map_->GetChipType(minX - 1, chipY_) == MapChip2D::ChipType::kLift)) {
        minX--;
    }
    while (maxX + 1 < map_->GetWidth() && (map_->GetChipType(maxX + 1, chipY_) == MapChip2D::ChipType::kRail || map_->GetChipType(maxX + 1, chipY_) == MapChip2D::ChipType::kLift)) {
        maxX++;
    }

    int minY = chipY_, maxY = chipY_;
    while (minY - 1 >= 0 && (map_->GetChipType(chipX_, minY - 1) == MapChip2D::ChipType::kRail || map_->GetChipType(chipX_, minY - 1) == MapChip2D::ChipType::kLift)) {
        minY--;
    }
    while (maxY + 1 < map_->GetHeight() && (map_->GetChipType(chipX_, maxY + 1) == MapChip2D::ChipType::kRail || map_->GetChipType(chipX_, maxY + 1) == MapChip2D::ChipType::kLift)) {
        maxY++;
    }

    if (maxX > minX) {
        direction_ = { 1.0f, 0.0f, 0.0f }; // 水平移動
        minRailWorldX_ = map_->ChipToWorldX(minX) + map_->GetChipSize() * 0.5f;
        maxRailWorldX_ = map_->ChipToWorldX(maxX) + map_->GetChipSize() * 0.5f;

        if (std::abs(worldX - minRailWorldX_) <= std::abs(worldX - maxRailWorldX_)) {
            startPos_ = { minRailWorldX_, worldY, 0.0f };
            endPos_ = { maxRailWorldX_, worldY, 0.0f };
        } else {
            startPos_ = { maxRailWorldX_, worldY, 0.0f };
            endPos_ = { minRailWorldX_, worldY, 0.0f };
        }
    } else if (maxY > minY) {
        direction_ = { 0.0f, 1.0f, 0.0f }; // 垂直移動
        minRailWorldY_ = map_->ChipToWorldY(minY) + map_->GetChipSize() * 0.5f;
        maxRailWorldY_ = map_->ChipToWorldY(maxY) + map_->GetChipSize() * 0.5f;

        if (std::abs(worldY - minRailWorldY_) <= std::abs(worldY - maxRailWorldY_)) {
            startPos_ = { worldX, minRailWorldY_, 0.0f };
            endPos_ = { worldX, maxRailWorldY_, 0.0f };
        } else {
            startPos_ = { worldX, maxRailWorldY_, 0.0f };
            endPos_ = { worldX, minRailWorldY_, 0.0f };
        }
    } else {
        direction_ = { 0.0f, 0.0f, 0.0f }; // レールがない場合は動かない
        startPos_ = { worldX, worldY, 0.0f };
        endPos_ = { worldX, worldY, 0.0f };
    }

    primitiveObj_->SetTranslation(startPos_);

    velocity_ = { 0.0f, 0.0f, 0.0f };
    currentT_ = 0.0f;
    state_ = LiftState::IdleAtStart;
    waitTimer_ = 0.0f;
    isPlayerStandingThisFrame_ = false;
}

void LiftBlock::Update() {
    float deltaTime = TimeManager::GetInstance().GetDeltaTime();
    if (deltaTime <= 0.0f) return;

    Vector3 prevPos = primitiveObj_->GetTranslation();

    // 距離を計算
    float distance = 0.0f;
    if (direction_.x != 0.0f) {
        distance = maxRailWorldX_ - minRailWorldX_;
    } else if (direction_.y != 0.0f) {
        distance = maxRailWorldY_ - minRailWorldY_;
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
            if (waitTimer_ >= 1.0f) { // 1秒待機
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
        // 往路はEaseInSine
        progress = 1.0f - std::cos((currentT_ * 3.14159265359f) / 2.0f);
    } else {
        // 復路および待機時はLinear
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

    primitiveObj_->SetTranslation(newPos);
    primitiveObj_->Update();

    // 1フレームのフラグをリセット
    isPlayerStandingThisFrame_ = false;
}
