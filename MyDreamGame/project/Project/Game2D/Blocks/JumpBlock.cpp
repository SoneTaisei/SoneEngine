#include "JumpBlock.h"
#include "../Player2D.h"

void JumpBlock::Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) {
    primitiveObj_ = std::make_unique<PrimitiveObject>();
    primitiveObj_->Initialize(device, boxPrimitive);
    
    // ジャンプ台の色：オレンジ色
    primitiveObj_->GetMaterial().color = { 1.0f, 0.5f, 0.0f, 1.0f };
    
    primitiveObj_->SetScale({ width, height, 1.0f });
    primitiveObj_->SetTranslation({ worldX, worldY, 0.0f });
    primitiveObj_->GetMaterial().lightingType = 0; // ライティング無効化
}

void JumpBlock::OnPlayerStand() {
    // プレイヤーが乗った時に処理される可能性があるが、
    // 確実に処理するために OnCollision でも判定を行うのが安全。
}

void JumpBlock::OnCollision(Player2D* player) {
    if (!player) return;

    // プレイヤーとジャンプ台の位置関係を取得
    Vector3 playerPos = player->GetPosition();
    Vector3 blockPos = primitiveObj_->GetTranslation();
    
    // プレイヤーの足元がジャンプ台より上にある場合のみ跳ねる（上から乗った判定）
    // マップチップサイズが1.0fなので、大体上側にいるか判定
    if (playerPos.y >= blockPos.y + 0.5f) {
        Vector3 vel = player->GetVelocity();
        vel.y = 12.5f; // 前回(6.25f)の2倍に設定。通常ジャンプ(10.0f)より少し高い。
        player->SetVelocity(vel);
        
        // 跳ねた演出として少しスケールを揺らす（実装できる範囲で）
        // primitiveObj_->SetScale({ primitiveObj_->GetScale().x * 1.2f, primitiveObj_->GetScale().y * 0.8f, 1.0f });
    }
}
