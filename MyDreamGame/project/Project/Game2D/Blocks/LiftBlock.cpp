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
    } else if (maxY > minY) {
        direction_ = { 0.0f, 1.0f, 0.0f }; // 垂直移動
        minRailWorldY_ = map_->ChipToWorldY(minY) + map_->GetChipSize() * 0.5f;
        maxRailWorldY_ = map_->ChipToWorldY(maxY) + map_->GetChipSize() * 0.5f;
    } else {
        direction_ = { 0.0f, 0.0f, 0.0f }; // レールがない場合は動かない
    }

    velocity_ = { direction_.x * speed_, direction_.y * speed_, 0.0f };
}

void LiftBlock::Update() {
    float deltaTime = TimeManager::GetInstance().GetDeltaTime();
    Vector3 pos = primitiveObj_->GetTranslation();
    
    pos.x += velocity_.x * deltaTime;
    pos.y += velocity_.y * deltaTime;

    if (direction_.x != 0.0f) {
        if (pos.x <= minRailWorldX_ || pos.x >= maxRailWorldX_) {
            direction_.x *= -1.0f;
            velocity_.x = direction_.x * speed_;
            pos.x = std::clamp(pos.x, minRailWorldX_, maxRailWorldX_);
        }
    } else if (direction_.y != 0.0f) {
        if (pos.y <= minRailWorldY_ || pos.y >= maxRailWorldY_) {
            direction_.y *= -1.0f;
            velocity_.y = direction_.y * speed_;
            pos.y = std::clamp(pos.y, minRailWorldY_, maxRailWorldY_);
        }
    }

    primitiveObj_->SetTranslation(pos);
    primitiveObj_->Update();
}
