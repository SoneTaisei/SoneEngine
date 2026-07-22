#include "JumpBlock.h"
#include "../Player/Player2D.h"
#include "../MapChip2D.h"

void JumpBlock::Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) {
    gameObject_ = std::make_unique<GameObject>("JumpBlock");
    auto* tc = gameObject_->AddComponent<TransformComponent>();
    auto* prc = gameObject_->AddComponent<PrimitiveRendererComponent>();

    prc->Initialize(device, boxPrimitive);
    
    // ジャンプ台の色Eオレンジ色
    prc->GetMaterial().color = { 1.0f, 0.5f, 0.0f, 1.0f };
    
    tc->SetScale({ width, height, 1.0f });
    tc->SetPosition({ worldX, worldY, 0.0f });
    prc->GetMaterial().lightingType = 1; // ライチEング無効匁E
    SetupCollider();
}

void JumpBlock::OnPlayerStand() {
    // プレイヤーが乗った時に処琁Eれる可能性があるが、E
    // 確実に処琁Eるために OnCollision でも判定を行うのが安E、E
}

void JumpBlock::OnCollision(Player2D* player) {
    if (!player) return;

    // ジャンプ台の AABB を取征E
    AABB2D blockAABB = GetAABB();
    // プレイヤーの AABB を取征E
    AABB2D playerAABB = player->GetAABB();
    
    // 周囲のブロチE��状況を取征E
    bool hasRight  = map_->GetBlock(chipX_ + 1, chipY_) != nullptr;
    bool hasLeft   = map_->GetBlock(chipX_ - 1, chipY_) != nullptr;
    bool hasTop    = map_->GetBlock(chipX_, chipY_ + 1) != nullptr;
    bool hasBottom = map_->GetBlock(chipX_, chipY_ - 1) != nullptr;

    bool isFloating = (!hasRight && !hasLeft && !hasTop && !hasBottom);

    // 接地面�E�ブロチE��がくっつぁE��ぁE��面�E�を優先度頁E��判定し、�Eねの方向を一つに絞る
    bool activeTop = false;
    bool activeBottom = false;
    bool activeLeft = false;
    bool activeRight = false;

    if (hasBottom) {
        activeTop = true; // 下にブロチE��があるなら上面で跳ねめE
    } else if (hasLeft) {
        activeRight = true; // 左にブロチE��があるなら右面で跳ねめE
    } else if (hasRight) {
        activeLeft = true; // 右にブロチE��があるなら左面で跳ねめE
    } else if (hasTop) {
        activeBottom = true; // 上にブロチE��があるなら下面で跳ねめE
    } else {
        activeTop = true; // 完�Eに浮ぁE��ぁE��場合�EチE��ォルトで上面で跳ねめE
    }

    // 吁E��との距離を計算！Elayer2D側でめり込みが押し戻されてぁE��ため、接触面は距離がほぼ0になる！E
    float distTop = std::abs(playerAABB.bottom - blockAABB.top);
    float distBottom = std::abs(playerAABB.top - blockAABB.bottom);
    float distLeft = std::abs(playerAABB.right - blockAABB.left);
    float distRight = std::abs(playerAABB.left - blockAABB.right);

    float minDist = (std::min)({ distTop, distBottom, distLeft, distRight });

    Vector3 vel = player->GetVelocity();
    const float threshold = 0.15f; // 接触判定E余裁E

    bool isGrazeTop = distTop < 0.5f && vel.y <= 0.0f;

    if ((minDist == distTop && distTop < threshold && activeTop) || 
        ((minDist == distLeft || minDist == distRight) && isGrazeTop && activeTop)) {
        
        // 判定バッファ：横からかすった場合のみ、ばねの上にキャラを吸い寄せる（位置補正）
        Vector3 pos = player->GetPosition();
        float playerHalfHeight = (playerAABB.top - playerAABB.bottom) * 0.5f;
        float playerHalfWidth = (playerAABB.right - playerAABB.left) * 0.5f;
        
        if (minDist == distLeft && isGrazeTop) {
            // 左からかすった場合、少しだけ右(内側)に寄せる
            pos.x = blockAABB.left - playerHalfWidth + 0.1f;
        } else if (minDist == distRight && isGrazeTop) {
            // 右からかすった場合、少しだけ左(内側)に寄せる
            pos.x = blockAABB.right + playerHalfWidth - 0.1f;
        }
        
        pos.y = blockAABB.top + playerHalfHeight;
        player->SetPosition(pos);

        // 1. 速度の上書き
        vel.y = jumpVelocityVertical_;
        player->SetVelocity(vel);
        player->SetIsOnGround(false);

        // 2. ダッシュ回数の全回復
        player->RefillDash();

        // 3. ヒットストップ（約1〜2フレーム：0.03秒）
        // (コントロール無効化は操作性が悪いため削除)
        player->ApplyHitstop(0.03f);

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
    // 古ぁE��式�E互換性維持E
    if (properties.contains("jumpVelocity")) {
        jumpVelocityVertical_ = properties["jumpVelocity"].get<float>();
        jumpVelocityHorizontal_ = properties["jumpVelocity"].get<float>();
    }
}
