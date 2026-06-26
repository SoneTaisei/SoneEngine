#include "RailBlock.h"

void RailBlock::Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) {
    primitiveObj_ = std::make_unique<PrimitiveObject>();
    primitiveObj_->Initialize(device, boxPrimitive);
    
    // レール：少し暗い灰色
    primitiveObj_->GetMaterial().color = { 0.4f, 0.4f, 0.4f, 1.0f };
    
    // 背景にあるように見せるため、少し奥に配置
    primitiveObj_->SetTranslation({ worldX, worldY, 0.5f });
    
    // レールとしての見た目を作るために少し細くする
    primitiveObj_->SetScale({ width * 0.2f, height * 0.2f, 1.0f });
    
    primitiveObj_->GetMaterial().lightingType = 0; // ライティング無効化
}
