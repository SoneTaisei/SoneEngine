#include "Treasure2D.h"
#include "GameObject/Object3D.h"
#include "Resource/Model/ModelManager.h"
#include "Renderer/DirectXCommon/DirectXCommon.h"
#include <cmath>

namespace {
    // ウズシオクリスタル色：鮮やかなサンセットアンバーオレンジ
    constexpr Vector4 kDefaultBaseColor = { 1.0f, 0.45f, 0.08f, 1.0f };
    // 回転中の合図色（より輝かしいゴールデンアンバー）
    constexpr Vector4 kDefaultHighlightColor = { 1.0f, 0.85f, 0.25f, 1.0f };
}

void Treasure2D::Initialize(const std::string& modelDir, const std::string& modelFile, float scale) {
    scale_ = scale;
    baseColor_ = kDefaultBaseColor;
    highlightColor_ = kDefaultHighlightColor;

    ID3D12Device* device = DirectXCommon::GetInstance()->GetDevice();
    Model* model = ModelManager::GetInstance()->GetModel(modelDir, modelFile);

    obj_ = std::make_unique<Object3D>();
    obj_->Initialize(device, model);
    obj_->SetName("Treasure");

    // ウズシオクリスタル用シェーディング（lightingType == 2: クリスタル/宝石モード）
    Material& mat = obj_->GetMaterial();
    mat.color = baseColor_;
    mat.lightingType = 2;              // Crystal / Gemstone Shading
    mat.enableEnvironmentMap = 1;      // 環境キューブマップの反射・フェイク屈折を有効化
    mat.shininess = 64.0f;             // 鋭い表面スペキュラ
    mat.environmentCoefficient = 0.8f; // 環境マップ映り込み係数

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
        obj_->GetMaterial().color = highlight_ ? highlightColor_ : baseColor_;
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
    obj_->SetTranslation({ pos.x, pos.y, drawOffsetZ_ });
    // 向きは最後の節の方向 + 自転（球では見えないが、正式モデルに差し替えた時のため）
    float angle = std::atan2(pos.y - prevNodePos.y, pos.x - prevNodePos.x);
    // 宝石の尖った側（モデルの -y）が鎖の外向き＝最後の節の方向を向くように吊る。自転は宝石の対称軸（y）まわり
    obj_->SetRotation({ 0.0f, selfAngle_, angle + 1.57079632f });
}

void Treasure2D::Draw() {
    if (obj_) {
        obj_->Draw();
    }
}

#ifdef USE_IMGUI
#include <imgui.h>

void Treasure2D::DrawImGui() {
    if (!obj_) return;
    ImGui::SeparatorText("Treasure (Crystal Material)");
    Material& mat = obj_->GetMaterial();
    
    if (ImGui::ColorEdit4("Base Color##Treasure", &baseColor_.x)) {
        if (!highlight_) mat.color = baseColor_;
    }
    if (ImGui::ColorEdit4("Highlight Color##Treasure", &highlightColor_.x)) {
        if (highlight_) mat.color = highlightColor_;
    }
    ImGui::SliderInt("Lighting Type##Treasure", &mat.lightingType, 0, 2);
    ImGui::SliderFloat("Shininess##Treasure", &mat.shininess, 1.0f, 256.0f);
    ImGui::SliderFloat("Env Coefficient##Treasure", &mat.environmentCoefficient, 0.0f, 2.0f);
    bool envMap = mat.enableEnvironmentMap != 0;
    if (ImGui::Checkbox("Enable Env Map##Treasure", &envMap)) {
        mat.enableEnvironmentMap = envMap ? 1 : 0;
    }
    ImGui::DragFloat("Visual Scale##Treasure", &scale_, 0.01f, 0.05f, 2.0f);
    if (obj_) obj_->SetScale({ scale_, scale_, scale_ });
    ImGui::DragFloat("Draw Offset Z##Treasure", &drawOffsetZ_, 0.01f, -2.0f, 2.0f);
}
#endif
