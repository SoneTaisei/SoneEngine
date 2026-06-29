#include "NormalBlock.h"

void NormalBlock::Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) {
    primitiveObj_ = std::make_unique<PrimitiveObject>();
    primitiveObj_->Initialize(device, boxPrimitive);
    if (chipY_ <= 1) {
        // 地面：茶色
        primitiveObj_->GetMaterial().color = { 0.55f, 0.35f, 0.17f, 1.0f };
    } else {
        // 壁：緑色
        primitiveObj_->GetMaterial().color = { 0.4f, 0.8f, 0.4f, 1.0f };
    }
    primitiveObj_->SetScale({ width, height, 1.0f });
    primitiveObj_->SetTranslation({ worldX, worldY, 0.0f });
    primitiveObj_->GetMaterial().lightingType = 1; // ライティング（鏡面反射など）を無効化
}
