#include "DeathBlock.h"
#include "Game2D/Player2D.h"

void DeathBlock::Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) {
    primitiveObj_ = std::make_unique<PrimitiveObject>();
    primitiveObj_->Initialize(device, boxPrimitive);
    primitiveObj_->GetMaterial().color = { 1.0f, 0.2f, 0.2f, 1.0f };
    primitiveObj_->SetScale({ width, height, 1.0f });
    primitiveObj_->SetTranslation({ worldX, worldY, 0.0f });
    primitiveObj_->GetMaterial().lightingType = 0; // ライティング無効化
}

void DeathBlock::OnCollision(Player2D* player) {
    if (player) {
        player->Kill();
    }
}
