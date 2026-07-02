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
    bool hasProp = false;
    if (properties.contains("direction")) {
        propDirection_ = properties["direction"].get<std::string>();
        hasProp = true;
    }
    if (properties.contains("range")) {
        propRange_ = properties["range"].get<float>();
        hasProp = true;
    }
    if (properties.contains("speed")) {
        propSpeed_ = properties["speed"].get<float>();
        hasProp = true;
    }

    if (hasProp) {
        useProperties_ = true;
        speedForward_ = propSpeed_;
        speedBackward_ = propSpeed_ * 0.5f;
        
        // レールが配置されておらず、移動距離が0になっている場合のみプロパティのrangeを適用
        if (std::abs(endPos_.x - startPos_.x) < 0.01f && std::abs(endPos_.y - startPos_.y) < 0.01f) {
            float rangeWorld = propRange_ * map_->GetChipSize();
            
            if (propDirection_ == "horizontal") {
                direction_ = { 1.0f, 0.0f, 0.0f };
                endPos_ = { startPos_.x + rangeWorld, startPos_.y, startPos_.z };
            } else if (propDirection_ == "vertical") {
                direction_ = { 0.0f, 1.0f, 0.0f };
                endPos_ = { startPos_.x, startPos_.y + rangeWorld, startPos_.z };
            } else {
                direction_ = { 0.0f, 0.0f, 0.0f };
                endPos_ = startPos_;
            }
        }
    }
}

void LiftBlock::Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) {
    primitiveObj_ = std::make_unique<PrimitiveObject>();
    primitiveObj_->Initialize(device, boxPrimitive);
    
    // リフトの色（例：明るい黄色やオレンジ）
    primitiveObj_->GetMaterial().color = { 0.9f, 0.6f, 0.1f, 1.0f };
    primitiveObj_->SetScale({ width, height, 1.0f });
    primitiveObj_->SetTranslation({ worldX, worldY, 0.0f });
    primitiveObj_->GetMaterial().lightingType = 1;

    // レールの範囲を探索して移動範囲を決定する
    int minX = chipX_, maxX = chipX_;
    while (minX - 1 >= 0 && IsRailOrLift(map_, minX - 1, chipY_)) {
        minX--;
    }
    while (maxX + 1 < map_->GetWidth() && IsRailOrLift(map_, maxX + 1, chipY_)) {
        maxX++;
    }

    int minY = chipY_, maxY = chipY_;
    while (minY - 1 >= 0 && IsRailOrLift(map_, chipX_, minY - 1)) {
        minY--;
    }
    while (maxY + 1 < map_->GetHeight() && IsRailOrLift(map_, chipX_, maxY + 1)) {
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
