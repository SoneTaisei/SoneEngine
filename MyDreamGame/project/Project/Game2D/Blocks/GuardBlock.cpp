#include "GuardBlock.h"
#include "Editor/Replay/ReplayManager.h"
#include "Game2D/MapChip2D.h"
#include "Game2D/Player/Player2D.h"
#include <cmath>
#ifdef USE_IMGUI
#include "Editor/EditorManager.h"
#endif

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
    sightRenderer->GetMaterial().lightingType = 1;

    SetupCollider();
    
    // プロパティ反映後に初期化
    direction_ = startDirection_;
    prevPosition_ = {worldX, worldY, 0.0f};
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
    if (properties.contains("sightHeight") && properties["sightHeight"].is_number()) {
        sightHeight_ = properties["sightHeight"];
    }
    if (properties.contains("maxAlertGauge") && properties["maxAlertGauge"].is_number()) {
        maxAlertGauge_ = properties["maxAlertGauge"];
    }
    if (properties.contains("startDirection") && properties["startDirection"].is_number()) {
        startDirection_ = properties["startDirection"];
        direction_ = startDirection_;
    }
    if (properties.contains("waitTimeAtEdge") && properties["waitTimeAtEdge"].is_number()) {
        waitTimeAtEdge_ = properties["waitTimeAtEdge"];
    }
}

void GuardBlock::Update() {
    BaseBlock::Update();
    if (!gameObject_) return;

    auto* tc = gameObject_->GetComponent<TransformComponent>();
    if (!tc) return;

    Vector3 currentPos = tc->GetPosition();

#ifdef USE_IMGUI
    // エディタ停止中であっても、視界オブジェクトの位置合わせや更新は行う
    if (!EditorManager::IsPlaying()) {
        if (sightObject_) {
            auto* sightTc = sightObject_->GetComponent<TransformComponent>();
            auto* sightRenderer = sightObject_->GetComponent<PrimitiveRendererComponent>();
            if (sightTc && sightRenderer) {
                sightTc->SetScale({sightLength_, sightHeight_, 1.0f});
                float sightOffsetX = (startWidth_ * 0.5f + sightLength_ * 0.5f) * currentFacing_;
                sightTc->SetPosition({currentPos.x + sightOffsetX, currentPos.y, 0.0f});
                sightRenderer->GetMaterial().color = {1.0f, 1.0f, 0.0f, 0.3f};
            }
            sightObject_->Update();
        }
        return;
    }
#endif

    // リプレイ再生・シーク時も録画時と同じだけ時間が進むよう、共有クロックの差分を使う
    float dt = ReplayManager::GetInstance()->GetPlayDeltaTime();
    if (dt <= 0.0f) return;

    // 向きの滑らかな補間
    currentFacing_ += (direction_ - currentFacing_) * 10.0f * dt;

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
            if (state_ == State::Alert) {
                state_ = State::Patrol;
            }
        }
    }

    float moveAmount = 0.0f;
    
    if (state_ == State::Wait) {
        // 待機中
        waitTimer_ -= dt;
        if (waitTimer_ <= 0.0f) {
            state_ = State::Patrol;
            direction_ *= -1; // 向きを反転
        }
    } else {
        // 移動中 (Patrol or Alert)
        float speed = (state_ == State::Alert) ? alertSpeed_ : patrolSpeed_;
        moveAmount = speed * dt * direction_;
    }

    Vector3 newPos = currentPos;
    newPos.x += moveAmount;

    // パトロール時の移動範囲制御（警戒時は範囲外まで追いかけるか？）
    // 警戒時でもパトロール範囲内で制限するかどうか。今回は範囲内とする。
    if (newPos.x > startX_ + moveRange_) {
        newPos.x = startX_ + moveRange_;
        if (state_ != State::Wait && state_ != State::Alert) {
            state_ = State::Wait;
            waitTimer_ = waitTimeAtEdge_;
        } else if (state_ == State::Alert) {
            // Alert中は待機せず端で止まる
        }
    } else if (newPos.x < startX_ - moveRange_) {
        newPos.x = startX_ - moveRange_;
        if (state_ != State::Wait && state_ != State::Alert) {
            state_ = State::Wait;
            waitTimer_ = waitTimeAtEdge_;
        } else if (state_ == State::Alert) {
            // Alert中は待機せず端で止まる
        }
    }

    deltaPosition_ = {newPos.x - prevPosition_.x, newPos.y - prevPosition_.y, 0.0f};
    currentVelocity_ = {deltaPosition_.x / dt, deltaPosition_.y / dt, 0.0f};
    prevPosition_ = newPos;

    tc->SetPosition(newPos);
    
    // 本体の滑らかな回転 (1.0 のとき 0度, -1.0 のとき 180度)
    float yaw = (1.0f - currentFacing_) * 0.5f * 3.14159265f;
    tc->SetRotation({0.0f, yaw, 0.0f});

    // 視界オブジェクトの更新
    if (sightObject_) {
        auto* sightTc = sightObject_->GetComponent<TransformComponent>();
        auto* sightRenderer = sightObject_->GetComponent<PrimitiveRendererComponent>();
        if (sightTc && sightRenderer) {
            // 視界のスケール（前方に伸びる、高さは sightHeight_）
            sightTc->SetScale({sightLength_, sightHeight_, 1.0f});
            // 視界の位置（本体の横から前方へ）
            float sightOffsetX = (startWidth_ * 0.5f + sightLength_ * 0.5f) * currentFacing_;
            sightTc->SetPosition({newPos.x + sightOffsetX, newPos.y, 0.0f});
            
            // 色の更新（黄色から赤へ）
            float alertRatio = maxAlertGauge_ > 0.0f ? (alertGauge_ / maxAlertGauge_) : 0.0f;
            sightRenderer->GetMaterial().color = {1.0f, 1.0f - alertRatio, 0.0f, 0.4f + alertRatio * 0.3f};
        }
        sightObject_->Update();
    }

    // 次のフレームのためのフラグリセット
    isPlayerInSightThisFrame_ = false;
}

void GuardBlock::Draw() {
    BaseBlock::Draw();
    if (sightObject_) {
        sightObject_->Draw();
    }
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
    if (direction_ == 1) {
        sightLeft = pos.x + startWidth_ * 0.5f;
        sightRight = sightLeft + sightLength_;
    } else {
        sightRight = pos.x - startWidth_ * 0.5f;
        sightLeft = sightRight - sightLength_;
    }
    float sightTop = pos.y + sightHeight_ * 0.5f;
    float sightBottom = pos.y - sightHeight_ * 0.5f;

    return {sightLeft, sightTop, sightRight, sightBottom};
}

void GuardBlock::OnSpottedPlayer(Player2D* player) {
    isPlayerInSightThisFrame_ = true;
    
    // ゲージがMAXなら即座にゲームオーバー
    if (alertGauge_ >= maxAlertGauge_ && player) {
        player->Kill(false);
    }
    
    // プレイヤーの方向に強制的に向きを変えて追跡する
    if (player && gameObject_) {
        auto* tc = gameObject_->GetComponent<TransformComponent>();
        if (tc) {
            float playerX = player->GetPosition().x;
            float guardX = tc->GetPosition().x;
            
            // プレイヤーが右にいれば右を向く、左にいれば左を向く
            if (playerX > guardX + 0.1f) {
                direction_ = 1;
            } else if (playerX < guardX - 0.1f) {
                direction_ = -1;
            }
        }
    }
}

void GuardBlock::Reset() {
    state_ = State::Patrol;
    alertGauge_ = 0.0f;
    direction_ = startDirection_;
    isPlayerInSightThisFrame_ = false;
    waitTimer_ = 0.0f;
    if (gameObject_) {
        auto* tc = gameObject_->GetComponent<TransformComponent>();
        if (tc) {
            tc->SetPosition({startX_, startY_, 0.0f});
            prevPosition_ = {startX_, startY_, 0.0f};
        }
    }
}

void GuardBlock::CaptureReplayState(std::vector<float>& outCustom) const {
    outCustom.clear();
    outCustom.push_back(static_cast<float>(state_));
    outCustom.push_back(static_cast<float>(direction_));
    outCustom.push_back(currentFacing_);
    outCustom.push_back(alertGauge_);
    outCustom.push_back(waitTimer_);
}

void GuardBlock::RestoreReplayState(const std::vector<float>& custom) {
    if (custom.size() < 5) return;
    state_ = static_cast<State>(static_cast<int>(custom[0]));
    direction_ = static_cast<int>(custom[1]);
    currentFacing_ = custom[2];
    alertGauge_ = custom[3];
    waitTimer_ = custom[4];

    // 位置は共通処理側で復元済みなので、速度の計算基準だけ合わせておく
    if (gameObject_) {
        if (auto* tc = gameObject_->GetComponent<TransformComponent>()) {
            prevPosition_ = tc->GetPosition();
        }
    }
    deltaPosition_ = {0.0f, 0.0f, 0.0f};
    currentVelocity_ = {0.0f, 0.0f, 0.0f};
}
