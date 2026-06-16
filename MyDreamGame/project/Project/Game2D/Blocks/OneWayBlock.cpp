#include "OneWayBlock.h"

void OneWayBlock::Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float size) {
    primitiveObj_ = std::make_unique<PrimitiveObject>();
    primitiveObj_->Initialize(device, boxPrimitive);
    primitiveObj_->GetMaterial().color = { 0.4f, 0.8f, 0.8f, 1.0f };
    primitiveObj_->SetScale({ size, size * 0.3f, size });
    primitiveObj_->SetTranslation({ worldX, worldY + size * 0.35f, 0.0f });
    primitiveObj_->GetMaterial().lightingType = 0; // ライティング無効化
}
