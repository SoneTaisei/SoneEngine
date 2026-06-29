#include "JumpBlock.h"
#include "../Player2D.h"

void JumpBlock::Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) {
    primitiveObj_ = std::make_unique<PrimitiveObject>();
    primitiveObj_->Initialize(device, boxPrimitive);
    
    // ジャンプ台の色：オレンジ色
    primitiveObj_->GetMaterial().color = { 1.0f, 0.5f, 0.0f, 1.0f };
    
    primitiveObj_->SetScale({ width, height, 1.0f });
    primitiveObj_->SetTranslation({ worldX, worldY, 0.0f });
    primitiveObj_->GetMaterial().lightingType = 1; // ライティング無効化
}

void JumpBlock::OnPlayerStand() {
    // プレイヤーが乗った時に処理される可能性があるが、
    // 確実に処理するために OnCollision でも判定を行うのが安全。
}

void JumpBlock::OnCollision(Player2D* player) {
    if (!player) return;

    // ジャンプ台の AABB を取得
    AABB2D blockAABB = GetAABB();
    // プレイヤーの AABB を取得
    AABB2D playerAABB = player->GetAABB();
    
    // プレイヤーの足元（底面）が、ジャンプ台の上面近辺かそれ以上にある場合のみ跳ねる
    if (playerAABB.bottom >= blockAABB.top - 0.1f) {
        Vector3 vel = player->GetVelocity();
        vel.y = jumpVelocity_; // カスタム可能にしたジャンプ威力
        player->SetVelocity(vel);
        
        // 跳ねた演出として少しスケールを揺らす（実装できる範囲で）
        // primitiveObj_->SetScale({ primitiveObj_->GetScale().x * 1.2f, primitiveObj_->GetScale().y * 0.8f, 1.0f });
    }
}

void JumpBlock::SetProperties(const nlohmann::json& properties) {
    if (properties.contains("jumpVelocity")) {
        jumpVelocity_ = properties["jumpVelocity"].get<float>();
    }
}
