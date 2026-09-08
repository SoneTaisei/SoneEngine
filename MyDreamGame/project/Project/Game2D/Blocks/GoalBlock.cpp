#include "GoalBlock.h"
#include "Game2D/Player/Player2D.h"
#include <cmath>

void GoalBlock::Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) {
    worldX_ = worldX;
    worldY_ = worldY;
    width_ = width;
    height_ = height;

    gameObject_ = std::make_unique<GameObject>("GoalBlock");
    auto* tc = gameObject_->AddComponent<TransformComponent>();
    auto* prc = gameObject_->AddComponent<PrimitiveRendererComponent>();

    prc->Initialize(device, boxPrimitive);
    prc->GetMaterial().color = { 0.8f, 0.2f, 0.8f, 1.0f }; // ゴールは紫色
    // 台座の見た目に合わせて下半分のボックスとして初期化
    tc->SetScale({ width, height * 0.5f, 1.0f });
    tc->SetPosition({ worldX, worldY - height * 0.25f, 0.0f });
    prc->GetMaterial().lightingType = 1; // ライティング無効化
    SetupCollider();
}

AABB2D GoalBlock::GetAABB() const {
    float halfW = width_ * 0.5f;
    float bottom = worldY_ - height_ * 0.5f;
    float top = worldY_; // 高さはマスの半分（底面〜マスの真ん中）
    return {
        worldX_ - halfW,
        top,
        worldX_ + halfW,
        bottom
    };
}

void GoalBlock::OnCollision(Player2D* player) {
    // 即時ゴールではなく、プレイヤーと宝石の両方が台座の上に乗った際に演出を開始するため
    // ここでは何もしない（GameScene::Update の CheckClearCondition で安全に判定する）
    (void)player;
}

bool GoalBlock::CheckClearCondition(const Vector3& playerPos, float playerHalfHeight, const Vector3& gemPos) const {
    float topY = GetTopY(); // 台座の上面Y座標
    float halfW = width_ * 0.85f; // 台座の上面判定幅（余裕を持たせる）

    // 1. プレイヤーが台座の上に立っている / 乗っているか（ジャンプ着地や縁立ちも許容）
    float playerFeetY = playerPos.y - playerHalfHeight;
    bool playerOnPedestal = (std::abs(playerPos.x - worldX_) <= halfW) &&
                            (playerFeetY >= topY - 0.40f && playerFeetY <= topY + 1.20f);

    // 2. 宝石が台座の上に乗っているか、またはプレイヤーが身につけて一緒に台座に乗っているか
    // （プレイヤーが宝石を持っていれば playerPos と gemPos が近いため即成立、置いてある場合も台座上判定で成立）
    float gemDistToPlayer = std::sqrt((gemPos.x - playerPos.x) * (gemPos.x - playerPos.x) + 
                                      (gemPos.y - playerPos.y) * (gemPos.y - playerPos.y));
    bool gemWithPlayer = (gemDistToPlayer <= 2.8f);
    bool gemOnPedestal = (std::abs(gemPos.x - worldX_) <= width_ * 1.0f) &&
                         (gemPos.y >= topY - 0.50f && gemPos.y <= topY + 2.50f);

    return playerOnPedestal && (gemWithPlayer || gemOnPedestal);
}
