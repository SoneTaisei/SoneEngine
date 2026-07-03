#include "JumpBlock.h"
#include "../Player2D.h"
#include "../MapChip2D.h"

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
    
    // 周囲のブロック状況を取得
    bool hasRight  = map_->GetBlock(chipX_ + 1, chipY_) != nullptr;
    bool hasLeft   = map_->GetBlock(chipX_ - 1, chipY_) != nullptr;
    bool hasTop    = map_->GetBlock(chipX_, chipY_ + 1) != nullptr;
    bool hasBottom = map_->GetBlock(chipX_, chipY_ - 1) != nullptr;

    bool isFloating = (!hasRight && !hasLeft && !hasTop && !hasBottom);

    // 有効な面（バウンドする面）を決定
    bool activeTop    = hasBottom || isFloating; // 下にブロックがある、または完全に浮いているなら上面で跳ねる
    bool activeBottom = hasTop;                  // 上にブロックがあるなら下面で跳ねる
    bool activeLeft   = hasRight;                // 右にブロックがあるなら左面で跳ねる（左向きのバネ）
    bool activeRight  = hasLeft;                 // 左にブロックがあるなら右面で跳ねる（右向きのバネ）

    // 各面との距離を計算（Player2D側でめり込みが押し戻されているため、接触面は距離がほぼ0になる）
    float distTop = std::abs(playerAABB.bottom - blockAABB.top);
    float distBottom = std::abs(playerAABB.top - blockAABB.bottom);
    float distLeft = std::abs(playerAABB.right - blockAABB.left);
    float distRight = std::abs(playerAABB.left - blockAABB.right);

    float minDist = (std::min)({ distTop, distBottom, distLeft, distRight });

    Vector3 vel = player->GetVelocity();
    const float threshold = 0.15f; // 接触判定の余裕

    if (minDist == distTop && distTop < threshold && activeTop) {
        vel.y = jumpVelocityVertical_;
        player->SetVelocity(vel);
    } else if (minDist == distBottom && distBottom < threshold && activeBottom) {
        vel.y = -jumpVelocityVertical_;
        player->SetVelocity(vel);
    } else if (minDist == distLeft && distLeft < threshold && activeLeft) {
        player->SetExternalVelocityX(-jumpVelocityHorizontal_);
        vel.y = 5.0f; // 少し上に浮かせることで接地判定を解除し、慣性がすぐに消されるのを防ぐ
        player->SetVelocity(vel);
        player->SetIsOnGround(false);
    } else if (minDist == distRight && distRight < threshold && activeRight) {
        player->SetExternalVelocityX(jumpVelocityHorizontal_);
        vel.y = 5.0f; // 少し上に浮かせる
        player->SetVelocity(vel);
        player->SetIsOnGround(false);
    }
}

void JumpBlock::SetProperties(const nlohmann::json& properties) {
    if (properties.contains("jumpVelocityVertical")) {
        jumpVelocityVertical_ = properties["jumpVelocityVertical"].get<float>();
    }
    if (properties.contains("jumpVelocityHorizontal")) {
        jumpVelocityHorizontal_ = properties["jumpVelocityHorizontal"].get<float>();
    }
    // 古い形式の互換性維持
    if (properties.contains("jumpVelocity")) {
        jumpVelocityVertical_ = properties["jumpVelocity"].get<float>();
        jumpVelocityHorizontal_ = properties["jumpVelocity"].get<float>();
    }
}
