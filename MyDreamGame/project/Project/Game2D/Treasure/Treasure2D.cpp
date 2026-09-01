#include "Treasure2D.h"
#include "GameObject/Object3D.h"
#include "Resource/Model/ModelManager.h"
#include "Renderer/DirectXCommon/DirectXCommon.h"
#include <cmath>

namespace {
    // 描画時のZオフセット（鎖(-0.2)よりわずかに手前に出して埋もれないようにする。物理はz=0のまま）
    constexpr float kDrawOffsetZ = -0.25f;
    // 仮モデル sphere.obj は半径1.0の単位球（OBJ実測値）なので、スケール = 表示半径
    constexpr float kModelRadius = 1.0f;
}

void Treasure2D::Initialize(float radius) {
    radius_ = radius;

    ID3D12Device* device = DirectXCommon::GetInstance()->GetDevice();
    Model* model = ModelManager::GetInstance()->GetModel("resources/Object/Original/sphere", "sphere.obj");

    obj_ = std::make_unique<Object3D>();
    obj_->Initialize(device, model);
    obj_->SetName("Treasure");
    obj_->GetMaterial().color = { 1.0f, 0.85f, 0.2f, 1.0f }; // 金色
    obj_->GetMaterial().lightingType = 1;
    SetRadius(radius_);
}

void Treasure2D::SetRadius(float radius) {
    radius_ = radius;
    if (obj_) {
        float s = radius_ / kModelRadius;
        obj_->SetScale({ s, s, s });
    }
}

void Treasure2D::UpdateTransform(const Vector3& pos, const Vector3& prevNodePos) {
    position_ = { pos.x, pos.y, 0.0f };
    if (!obj_) {
        return;
    }
    obj_->SetTranslation({ pos.x, pos.y, kDrawOffsetZ });
    // 向きは最後の節の方向（球では見えないが、正式モデルに差し替えた時のため）
    float angle = std::atan2(pos.y - prevNodePos.y, pos.x - prevNodePos.x);
    obj_->SetRotation({ 0.0f, 0.0f, angle });
}

void Treasure2D::Draw() {
    if (obj_) {
        obj_->Draw();
    }
}
