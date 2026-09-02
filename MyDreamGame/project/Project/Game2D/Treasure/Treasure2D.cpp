#include "Treasure2D.h"
#include "GameObject/Object3D.h"
#include "Resource/Model/ModelManager.h"
#include "Renderer/DirectXCommon/DirectXCommon.h"
#include <cmath>

namespace {
    // 描画時のZオフセット（鎖(-0.2)よりわずかに手前に出して埋もれないようにする。物理はz=0のまま）
    constexpr float kDrawOffsetZ = -0.25f;
    // 通常色（金）と回転中の合図色（明るい金）
    constexpr Vector4 kBaseColor = { 1.0f, 0.85f, 0.2f, 1.0f };
    constexpr Vector4 kHighlightColor = { 1.0f, 1.0f, 0.6f, 1.0f };
}

void Treasure2D::Initialize(const std::string& modelDir, const std::string& modelFile, float scale) {
    scale_ = scale;

    ID3D12Device* device = DirectXCommon::GetInstance()->GetDevice();
    Model* model = ModelManager::GetInstance()->GetModel(modelDir, modelFile);

    obj_ = std::make_unique<Object3D>();
    obj_->Initialize(device, model);
    obj_->SetName("Treasure");
    obj_->GetMaterial().color = kBaseColor;
    obj_->GetMaterial().lightingType = 1;
    SetVisualScale(scale_);
}

void Treasure2D::SetVisualScale(float scale) {
    scale_ = scale;
    if (obj_) {
        obj_->SetScale({ scale_, scale_, scale_ });
    }
}

void Treasure2D::SetHighlight(bool highlight) {
    if (highlight_ == highlight) {
        return;
    }
    highlight_ = highlight;
    if (obj_) {
        obj_->GetMaterial().color = highlight_ ? kHighlightColor : kBaseColor;
    }
}

void Treasure2D::AddSelfRotation(float deltaAngle) {
    // 無限に蓄積すると浮動小数の精度が落ちるので [-π, π] に巻き戻す
    constexpr float kTwoPi = 6.28318530718f;
    selfAngle_ += deltaAngle;
    if (selfAngle_ > kTwoPi * 0.5f) selfAngle_ -= kTwoPi;
    if (selfAngle_ < -kTwoPi * 0.5f) selfAngle_ += kTwoPi;
}

void Treasure2D::UpdateTransform(const Vector3& pos, const Vector3& prevNodePos) {
    position_ = { pos.x, pos.y, 0.0f };
    if (!obj_) {
        return;
    }
    obj_->SetTranslation({ pos.x, pos.y, kDrawOffsetZ });
    // 向きは最後の節の方向 + 自転（球では見えないが、正式モデルに差し替えた時のため）
    float angle = std::atan2(pos.y - prevNodePos.y, pos.x - prevNodePos.x);
    obj_->SetRotation({ 0.0f, 0.0f, angle + selfAngle_ });
}

void Treasure2D::Draw() {
    if (obj_) {
        obj_->Draw();
    }
}
