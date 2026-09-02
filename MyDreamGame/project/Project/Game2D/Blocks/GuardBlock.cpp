#include "GuardBlock.h"
#include "Core/TimeManager.h"
#include "Game2D/MapChip2D.h"
#include "Game2D/Player/Player2D.h"

GuardBlock::GuardBlock(MapChip2D* map, int chipX, int chipY)
    : BaseBlock(map, chipX, chipY) {}

void GuardBlock::Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) {
    startX_ = worldX;
    startY_ = worldY;
    startWidth_ = width;
    startHeight_ = height;

    gameObject_ = std::make_unique<GameObject>();
    gameObject_->Initialize();
    gameObject_->SetName("GuardBlock");

    auto* transform = gameObject_->AddComponent<TransformComponent>();
    transform->SetPosition({worldX, worldY, 0.0f});
    transform->SetScale({width, height, 1.0f});

    auto* renderer = gameObject_->AddComponent<PrimitiveRendererComponent>();
    renderer->Initialize(device, boxPrimitive);
    // 警備員の色（ダークブルー）
    renderer->GetMaterial().color = {0.1f, 0.2f, 0.5f, 1.0f};
    renderer->GetMaterial().lightingType = 1;

    // 視界オブジェクト
    sightObject_ = std::make_unique<GameObject>();
    sightObject_->Initialize();
    sightObject_->SetName("GuardSight");
    auto* sightTransform = sightObject_->AddComponent<TransformComponent>();
    sightTransform->SetPosition({worldX, worldY, 0.0f});
    auto* sightRenderer = sightObject_->AddComponent<PrimitiveRendererComponent>();
    sightRenderer->Initialize(device, boxPrimitive);
    // 視界の色（半透明の黄色）
    sightRenderer->GetMaterial().color = {1.0f, 1.0f, 0.0f, 0.3f};
    sightRenderer->GetMaterial().lightingType = 1; // 無効化してフラットにしても良いが一旦Lighting1

    SetupCollider();
}

void GuardBlock::SetProperties(const nlohmann::json& properties) {
    if (properties.contains("moveRange") && properties["moveRange"].is_number()) {
        moveRange_ = properties["moveRange"];
    }
    if (properties.contains("patrolSpeed") && properties["patrolSpeed"].is_number()) {
        patrolSpeed_ = properties["patrolSpeed"];
    }
    if (properties.contains("alertSpeed") && properties["alertSpeed"].is_number()) {
        alertSpeed_ = properties["alertSpeed"];
    }
    if (properties.contains("sightLength") && properties["sightLength"].is_number()) {
        sightLength_ = properties["sightLength"];
    }
    if (properties.contains("maxAlertGauge") && properties["maxAlertGauge"].is_number()) {
        maxAlertGauge_ = properties["maxAlertGauge"];
    }
}

void GuardBlock::Update() {
    BaseBlock::Update();
    if (!gameObject_) return;

    float dt = TimeManager::GetInstance().GetDeltaTime();
    if (dt <= 0.0f) return;

    auto* tc = gameObject_->GetComponent<TransformComponent>();
    if (!tc) return;

    Vector3 currentPos = tc->GetPosition();

    // 状態の更新
    if (isPlayerInSightThisFrame_) {
        state_ = State::Alert;
        alertGauge_ += dt;
        if (alertGauge_ >= maxAlertGauge_) {
            alertGauge_ = maxAlertGauge_;
        }
    } else {
        alertGauge_ -= dt * 0.5f; // 見失うと徐々に下がる
        if (alertGauge_ <= 0.0f) {
            alertGauge_ = 0.0f;
            state_ = State::Patrol;
        }
    }

    // 移動の更新
    float speed = (state_ == State::Alert) ? alertSpeed_ : patrolSpeed_;
    float moveAmount = speed * dt * direction_;
    
    Vector3 newPos = currentPos;
    newPos.x += moveAmount;

    // 移動範囲（startX_からの距離）を超えたら反転
    if (newPos.x > startX_ + moveRange_) {
        newPos.x = startX_ + moveRange_;
        direction_ = -1;
    } else if (newPos.x < startX_ - moveRange_) {
        newPos.x = startX_ - moveRange_;
        direction_ = 1;
    }

    deltaPosition_ = {newPos.x - prevPosition_.x, newPos.y - prevPosition_.y, 0.0f};
    currentVelocity_ = {deltaPosition_.x / dt, deltaPosition_.y / dt, 0.0f};
    prevPosition_ = newPos;

    tc->SetPosition(newPos);

    // 視界オブジェクトの更新
    if (sightObject_) {
        auto* sightTc = sightObject_->GetComponent<TransformComponent>();
        auto* sightRenderer = sightObject_->GetComponent<PrimitiveRendererComponent>();
        if (sightTc && sightRenderer) {
            // 視界のスケール（前方に伸びる）
            sightTc->SetScale({sightLength_, startHeight_ * 0.8f, 1.0f});
            // 視界の位置（本体の横から前方へ）
            float sightOffsetX = (startWidth_ * 0.5f + sightLength_ * 0.5f) * direction_;
            sightTc->SetPosition({newPos.x + sightOffsetX, newPos.y, 0.0f});
            
            // 色の更新（黄色から赤へ）
            float alertRatio = maxAlertGauge_ > 0.0f ? (alertGauge_ / maxAlertGauge_) : 0.0f;
            // 黄(1,1,0) -> 赤(1,0,0)
            sightRenderer->GetMaterial().color = {1.0f, 1.0f - alertRatio, 0.0f, 0.4f + alertRatio * 0.3f};
        }
        sightObject_->Update();
    }

    // 次のフレームのためのフラグリセット
    isPlayerInSightThisFrame_ = false;
}

void GuardBlock::OnCollision(Player2D* player) {
    if (player) {
        player->Kill(false);
    }
}

AABB2D GuardBlock::GetSightAABB() const {
    if (!gameObject_) return {0,0,0,0};
    auto* tc = gameObject_->GetComponent<TransformComponent>();
    if (!tc) return {0,0,0,0};

    Vector3 pos = tc->GetPosition();
    float sightLeft, sightRight;
    if (direction_ == 1) { // 右向き
        sightLeft = pos.x + startWidth_ * 0.5f;
        sightRight = sightLeft + sightLength_;
    } else { // 左向き
        sightRight = pos.x - startWidth_ * 0.5f;
        sightLeft = sightRight - sightLength_;
    }
    float sightTop = pos.y + startHeight_ * 0.4f;
    float sightBottom = pos.y - startHeight_ * 0.4f;

    return {sightLeft, sightTop, sightRight, sightBottom};
}

void GuardBlock::OnSpottedPlayer(Player2D* player) {
    isPlayerInSightThisFrame_ = true;
    
    // ゲージがMAXなら即座にゲームオーバー
    if (alertGauge_ >= maxAlertGauge_ && player) {
        player->Kill(false);
    }
    
    // プレイヤーの方向に強制的に向きを変える
    if (player && gameObject_) {
        auto* tc = gameObject_->GetComponent<TransformComponent>();
        if (tc) {
            float pX = player->GetVelocity().x; // Playerの位置を取る方法がないため本来は引数からか状態から？
            // PlayerPhysics側で AABB が重なっていたら呼ばれるので、位置の比較はできるかも。
            // しかし Player2D の位置は取れない。
            // とりあえず見つけた方向に進むのは AlertSpeed と同方向なら良いのでそのまま。
        }
    }
}

void GuardBlock::Reset() {
    state_ = State::Patrol;
    alertGauge_ = 0.0f;
    direction_ = 1;
    isPlayerInSightThisFrame_ = false;
    if (gameObject_) {
        auto* tc = gameObject_->GetComponent<TransformComponent>();
        if (tc) {
            tc->SetPosition({startX_, startY_, 0.0f});
            prevPosition_ = {startX_, startY_, 0.0f};
        }
    }
}
void GuardBlock::Draw() {
    BaseBlock::Draw();
    if (sightObject_) {
        sightObject_->Draw();
    }
}