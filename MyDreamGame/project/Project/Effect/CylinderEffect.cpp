#include "CylinderEffect.h"
#include "Resource/Primitive/PrimitiveManager.h"
#include "Graphics/TextureManager.h"
#include "Core/TimeManager.h"

CylinderEffect::CylinderEffect() {}

void CylinderEffect::Initialize(ID3D12Device* device, uint32_t textureHandle) {
    // ■ CylinderEffectのルートオブジェクト作成
    cylinderEffectRoot_ = std::make_unique<PrimitiveObject>();
    cylinderEffectRoot_->Initialize(device, nullptr); // 描画しない
    cylinderEffectRoot_->SetName("CylinderEffect");
    cylinderEffectRoot_->SetTranslation({0.0f, 0.0f, 0.0f});

    // Cylinder
    {
        cylinderObject_ = std::make_unique<PrimitiveObject>();
        cylinderObject_->Initialize(device, PrimitiveManager::GetInstance()->GetPrimitive(PrimitiveType::Cylinder, 1.0f, 64));
        cylinderObject_->GetMaterial().enableEnvironmentMap = 0;
        cylinderObject_->GetMaterial().lightingType = 0;
        cylinderObject_->SetTextureHandle(TextureManager::GetInstance()->GetGpuHandle(textureHandle));
        cylinderObject_->SetTranslation({0.0f, 0.0f, 0.0f});
        cylinderObject_->SetScale({1.5f, 2.0f, 1.5f});
        cylinderObject_->SetRotation({0.0f, 0.0f, 0.0f});
        cylinderObject_->SetIsBillboard(false);
        cylinderObject_->SetIsDoubleSided(true);
        cylinderObject_->SetBlendMode(BlendMode::kBlendModeAdd);
        cylinderObject_->GetMaterial().alphaReference = 0.0f;
        cylinderObject_->SetName("Cylinder");
        cylinderObject_->SetParent(cylinderEffectRoot_.get());
    }
}

void CylinderEffect::Update(float deltaTime) {
    if (cylinderEffectRoot_) {
        cylinderEffectRoot_->Update();
    }
    // シリンダーは点滅させずに回転させる
    if (cylinderObject_) {
        Vector3 rotation = cylinderObject_->GetRotation();
        rotation.y += 0.6f * deltaTime; // 毎フレーム固定値ではなく、デルタタイムに依存して回転させる
        cylinderObject_->SetRotation(rotation);
        cylinderObject_->Update();
    }
}

void CylinderEffect::Draw(ID3D12GraphicsCommandList* commandList) {
    if (cylinderObject_) {
        cylinderObject_->Draw(commandList);
    }
}
