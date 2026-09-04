#include "GuardBlock.h"
#include "Editor/Replay/ReplayManager.h"
#include "Game2D/MapChip2D.h"
#include "Game2D/Player/Player2D.h"
#include "GameObject/Object3D.h"
#include "Resource/Model/ModelManager.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#ifdef USE_IMGUI
#include "Editor/EditorManager.h"
#endif

namespace {
    constexpr float kPi = std::numbers::pi_v<float>;
    constexpr Vector4 kBodyColor = {0.1f, 0.2f, 0.5f, 1.0f};      // 通常
    constexpr Vector4 kStunColor = {0.45f, 0.5f, 0.7f, 1.0f};     // 気絶（薄い）
    constexpr Vector4 kBoundColor = {0.35f, 0.35f, 0.4f, 1.0f};   // 縛られ（灰）
    constexpr Vector4 kStaggerColor = {0.3f, 0.35f, 0.65f, 1.0f}; // よろけ
    constexpr int kBoundLinkCount = 6;                            // 体を囲む鎖のリンク数
    constexpr float kLinkModelLength = 0.468f;                    // kusari_yoko の長軸実寸
    constexpr float kUnbindGrace = 1.0f;                          // 縛りを解いてから気絶に落ちるまでの猶予
}

GuardBlock::GuardBlock(MapChip2D* map, int chipX, int chipY)
    : BaseBlock(map, chipX, chipY) {}

GuardBlock::~GuardBlock() = default;

void GuardBlock::Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) {
    startX_ = worldX;
    startY_ = worldY;
    startWidth_ = width;
    startHeight_ = height;
    device_ = device;

    gameObject_ = std::make_unique<GameObject>();
    gameObject_->Initialize();
    gameObject_->SetName("GuardBlock");

    auto* transform = gameObject_->AddComponent<TransformComponent>();
    transform->SetPosition({worldX, worldY, 0.0f});
    transform->SetScale({width, height, 1.0f});

    auto* renderer = gameObject_->AddComponent<PrimitiveRendererComponent>();
    renderer->Initialize(device, boxPrimitive);
    // 警備員の色（ダークブルー）
    renderer->GetMaterial().color = kBodyColor;
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
    auto readF = [&](const char* key, float& out) {
        if (properties.contains(key) && properties[key].is_number()) out = properties[key];
    };
    readF("moveRange", moveRange_);
    readF("patrolSpeed", patrolSpeed_);
    readF("alertSpeed", alertSpeed_);
    readF("sightLength", sightLength_);
    readF("sightHeight", sightHeight_);
    readF("maxAlertGauge", maxAlertGauge_);
    if (properties.contains("startDirection") && properties["startDirection"].is_number()) {
        startDirection_ = properties["startDirection"];
        direction_ = startDirection_;
    }
    readF("waitTimeAtEdge", waitTimeAtEdge_);
    // 鎖で倒す
    readF("stunSpeed", stunSpeed_);
    readF("stunBase", stunBase_);
    readF("stunPerSpeed", stunPerSpeed_);
    readF("stunMax", stunMax_);
    readF("staggerTime", staggerTime_);
    readF("wakeWarning", wakeWarning_);
    readF("hitCooldown", hitCooldown_);
    readF("tripStun", tripStun_);
    readF("tripSpeedBonus", tripSpeedBonus_);
    readF("tripCooldown", tripCooldown_);
    readF("tripFootHeight", tripFootHeight_);
    readF("unbindStun", unbindStun_);
    if (properties.contains("bindFromBehind") && properties["bindFromBehind"].is_boolean()) {
        bindFromBehind_ = properties["bindFromBehind"];
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

    // クールダウン類
    if (hitTimer_ > 0.0f) hitTimer_ -= dt;
    if (tripTimer_ > 0.0f) tripTimer_ -= dt;
    if (staggerTimer_ > 0.0f) staggerTimer_ -= dt;
    wobbleTime_ += dt;

    // 向きの滑らかな補間
    currentFacing_ += (direction_ - currentFacing_) * 10.0f * dt;

    float moveAmount = 0.0f;
    float targetTumble = 0.0f;
    Vector4 bodyColor = kBodyColor;

    if (state_ == State::Stunned) {
        // 気絶：動かない。起きる直前は点滅で予告
        stunTimer_ -= dt;
        targetTumble = static_cast<float>(direction_); // 進行方向へ倒れる
        bodyColor = kStunColor;
        if (stunTimer_ <= wakeWarning_) {
            float blink = std::sin(wobbleTime_ * 30.0f);
            bodyColor = (blink > 0.0f) ? kBodyColor : kStunColor;
        }
        if (stunTimer_ <= 0.0f) {
            state_ = State::Patrol;
            alertGauge_ = 0.0f;
        }
        isPlayerInSightThisFrame_ = false;
    } else if (state_ == State::Bound) {
        // 縛られ：動かない。もぞもぞ
        targetTumble = static_cast<float>(direction_) * 0.9f + std::sin(wobbleTime_ * 6.0f) * 0.05f;
        bodyColor = kBoundColor;
        isPlayerInSightThisFrame_ = false;
    } else {
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

        if (state_ == State::Wait) {
            // 待機中
            waitTimer_ -= dt;
            if (waitTimer_ <= 0.0f) {
                state_ = State::Patrol;
                direction_ *= -1; // 向きを反転
            }
        } else if (staggerTimer_ > 0.0f) {
            // よろけ：一瞬止まる（視界は消えない）
            bodyColor = kStaggerColor;
        } else {
            // 移動中 (Patrol or Alert)
            float speed = (state_ == State::Alert) ? alertSpeed_ : patrolSpeed_;
            moveAmount = speed * dt * direction_;
        }
    }

    Vector3 newPos = currentPos;
    newPos.x += moveAmount;

    // パトロール時の移動範囲制御（警戒時は範囲外まで追いかけるか？）
    // 警戒時でもパトロール範囲内で制限するかどうか。今回は範囲内とする。
    if (newPos.x > startX_ + moveRange_) {
        newPos.x = startX_ + moveRange_;
        if (state_ == State::Patrol) {
            state_ = State::Wait;
            waitTimer_ = waitTimeAtEdge_;
        }
    } else if (newPos.x < startX_ - moveRange_) {
        newPos.x = startX_ - moveRange_;
        if (state_ == State::Patrol) {
            state_ = State::Wait;
            waitTimer_ = waitTimeAtEdge_;
        }
    }

    deltaPosition_ = {newPos.x - prevPosition_.x, newPos.y - prevPosition_.y, 0.0f};
    currentVelocity_ = {deltaPosition_.x / dt, deltaPosition_.y / dt, 0.0f};
    prevPosition_ = newPos;

    tc->SetPosition(newPos);

    // 本体の滑らかな回転 (1.0 のとき 0度, -1.0 のとき 180度) + 倒れ
    tumble_ += (targetTumble - tumble_) * std::clamp(12.0f * dt, 0.0f, 1.0f);
    float yaw = (1.0f - currentFacing_) * 0.5f * kPi;
    tc->SetRotation({0.0f, yaw, -tumble_ * kPi * 0.5f});
    if (prompt_) {
        // 縛れる／取り戻せる合図：明るくする（押す前に結果が分かるように）
        bodyColor = {bodyColor.x + 0.35f, bodyColor.y + 0.35f, bodyColor.z + 0.25f, 1.0f};
    }
    prompt_ = false;
    if (auto* renderer = gameObject_->GetComponent<PrimitiveRendererComponent>()) {
        renderer->GetMaterial().color = bodyColor;
    }

    // 視界オブジェクトの更新（気絶・縛られ中は消す）
    if (sightObject_) {
        auto* sightTc = sightObject_->GetComponent<TransformComponent>();
        auto* sightRenderer = sightObject_->GetComponent<PrimitiveRendererComponent>();
        if (sightTc && sightRenderer) {
            if (IsIncapacitated()) {
                sightTc->SetScale({0.0f, 0.0f, 1.0f});
            } else {
                // 視界のスケール（前方に伸びる、高さは sightHeight_）
                sightTc->SetScale({sightLength_, sightHeight_, 1.0f});
                // 視界の位置（本体の横から前方へ）
                float sightOffsetX = (startWidth_ * 0.5f + sightLength_ * 0.5f) * currentFacing_;
                sightTc->SetPosition({newPos.x + sightOffsetX, newPos.y, 0.0f});

                // 色の更新（黄色から赤へ）
                float alertRatio = maxAlertGauge_ > 0.0f ? (alertGauge_ / maxAlertGauge_) : 0.0f;
                sightRenderer->GetMaterial().color = {1.0f, 1.0f - alertRatio, 0.0f, 0.4f + alertRatio * 0.3f};
            }
        }
        sightObject_->Update();
    }

    if (state_ == State::Bound) {
        UpdateBoundRing();
    }

    // 次のフレームのためのフラグリセット
    isPlayerInSightThisFrame_ = false;
}

void GuardBlock::UpdateBoundRing() {
    if (!device_) return;
    if (boundLinks_.empty()) {
        Model* model = ModelManager::GetInstance()->GetModel("resources/Object/Original/kusari/kusari_yoko", "kusari_yoko.obj");
        if (!model) return;
        for (int i = 0; i < kBoundLinkCount; ++i) {
            auto obj = std::make_unique<Object3D>();
            obj->Initialize(device_, model);
            obj->SetName("GuardBoundLink" + std::to_string(i));
            obj->GetMaterial().lightingType = 1;
            boundLinks_.push_back(std::move(obj));
        }
    }
    // 体（倒れているので横長）を囲む楕円にリンクを並べる
    AABB2D box = GetAABB();
    float cx = (box.left + box.right) * 0.5f;
    float cy = (box.top + box.bottom) * 0.5f;
    float a = (box.right - box.left) * 0.5f + 0.15f; // 横半径
    float b = (box.top - box.bottom) * 0.5f + 0.15f; // 縦半径
    float perimeter = 2.0f * kPi * std::sqrt((a * a + b * b) * 0.5f);
    float lengthScale = (perimeter / kBoundLinkCount * 1.15f) / kLinkModelLength;
    for (int i = 0; i < static_cast<int>(boundLinks_.size()); ++i) {
        float t = 2.0f * kPi * static_cast<float>(i) / static_cast<float>(kBoundLinkCount) + wobbleTime_ * 0.3f;
        float px = cx + a * std::cos(t);
        float py = cy + b * std::sin(t);
        float tangent = std::atan2(b * std::cos(t), -a * std::sin(t));
        Object3D* link = boundLinks_[i].get();
        link->SetTranslation({px, py, -0.55f}); // 体（前面 -0.5）より手前
        link->SetRotation({-kPi * 0.5f, 0.0f, tangent - kPi * 0.5f});
        link->SetScale({1.0f, 1.0f, lengthScale});
        link->Update();
    }
}

void GuardBlock::Draw() {
    BaseBlock::Draw();
    if (sightObject_ && !IsIncapacitated()) {
        sightObject_->Draw();
    }
    if (state_ == State::Bound) {
        for (auto& link : boundLinks_) {
            link->Draw();
        }
    }
}

void GuardBlock::OnCollision(Player2D* player) {
    // 気絶・縛られ中は触れても安全（縛る・拾い戻すために重なる必要がある）
    if (IsIncapacitated()) return;
    if (player) {
        player->Kill(false);
    }
}

AABB2D GuardBlock::GetSightAABB() const {
    if (!gameObject_ || IsIncapacitated()) return {-10000.0f, -10000.0f, -10000.0f, -10000.0f};
    auto* tc = gameObject_->GetComponent<TransformComponent>();
    if (!tc) return {-10000.0f, -10000.0f, -10000.0f, -10000.0f};

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
    if (IsIncapacitated()) return;
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

// ---------------------------------------------------------------------------
// 鎖で倒す
// ---------------------------------------------------------------------------

void GuardBlock::EnterStunned(float duration) {
    state_ = State::Stunned;
    stunTimer_ = (std::max)(0.1f, duration);
    alertGauge_ = 0.0f;
    isPlayerInSightThisFrame_ = false;
    staggerTimer_ = 0.0f;
}

bool GuardBlock::HitByTreasure(const Vector3& velocity) {
    if (state_ == State::Bound || hitTimer_ > 0.0f) {
        return false;
    }
    float speed = std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y);
    if (speed >= stunSpeed_) {
        float duration = std::clamp(stunBase_ + (speed - stunSpeed_) * stunPerSpeed_, 0.0f, stunMax_);
        if (state_ == State::Stunned) {
            stunTimer_ = (std::max)(stunTimer_, duration); // 気絶中に追撃されたら延長
        } else {
            EnterStunned(duration);
        }
        // 速度方向へ少しノックバック（巡回範囲でクランプ）
        if (gameObject_ && speed > 1e-3f) {
            if (auto* tc = gameObject_->GetComponent<TransformComponent>()) {
                Vector3 p = tc->GetPosition();
                p.x += velocity.x / speed * 0.5f;
                p.x = std::clamp(p.x, startX_ - moveRange_, startX_ + moveRange_);
                tc->SetPosition(p);
                prevPosition_ = p;
            }
        }
        hitTimer_ = hitCooldown_;
        return true;
    }
    // 不発：よろける（「当たったのに無反応」を避ける）
    if (state_ != State::Stunned && speed >= 1.0f) {
        staggerTimer_ = staggerTime_;
        hitTimer_ = hitCooldown_;
    }
    return false;
}

bool GuardBlock::TripByChain(float chainSpeed) {
    if (!IsMovingState() || tripTimer_ > 0.0f) {
        return false;
    }
    (void)chainSpeed;
    float speed = std::fabs(currentVelocity_.x);
    float duration = tripStun_ + speed * tripSpeedBonus_; // 走っているほど長く転ぶ
    float keepAlert = alertGauge_;
    EnterStunned(duration);
    alertGauge_ = keepAlert; // 転んでも警戒は続く
    tripTimer_ = tripCooldown_;
    return true;
}

bool GuardBlock::CanBind(const Vector3& playerPos) const {
    if (state_ == State::Stunned) {
        return true;
    }
    if (!bindFromBehind_ || !gameObject_) {
        return false;
    }
    if (state_ != State::Patrol && state_ != State::Wait) {
        return false; // 警戒中は不可
    }
    auto* tc = gameObject_->GetComponent<TransformComponent>();
    if (!tc) return false;
    float dx = playerPos.x - tc->GetPosition().x;
    // 背後 = 向いている方向の逆側
    return (direction_ > 0) ? (dx < 0.0f) : (dx > 0.0f);
}

void GuardBlock::Bind(int units) {
    state_ = State::Bound;
    boundUnits_ = (std::max)(1, units);
    stunTimer_ = 0.0f;
    alertGauge_ = 0.0f;
    isPlayerInSightThisFrame_ = false;
}

int GuardBlock::Unbind() {
    if (state_ != State::Bound) {
        return 0;
    }
    int units = boundUnits_;
    boundUnits_ = 0;
    // 猶予の後に短い気絶 → 起きる（拾ってすぐ逃げれば間に合う）
    EnterStunned(kUnbindGrace + unbindStun_);
    return units;
}

AABB2D GuardBlock::GetFootAABB() const {
    AABB2D box = GetAABB();
    return {box.left, box.bottom + tripFootHeight_, box.right, box.bottom};
}

// ---------------------------------------------------------------------------

void GuardBlock::Reset() {
    state_ = State::Patrol;
    alertGauge_ = 0.0f;
    direction_ = startDirection_;
    isPlayerInSightThisFrame_ = false;
    waitTimer_ = 0.0f;
    stunTimer_ = 0.0f;
    staggerTimer_ = 0.0f;
    hitTimer_ = 0.0f;
    tripTimer_ = 0.0f;
    boundUnits_ = 0;
    tumble_ = 0.0f;
    if (gameObject_) {
        auto* tc = gameObject_->GetComponent<TransformComponent>();
        if (tc) {
            tc->SetPosition({startX_, startY_, 0.0f});
            tc->SetRotation({0.0f, 0.0f, 0.0f});
            prevPosition_ = {startX_, startY_, 0.0f};
        }
        if (auto* renderer = gameObject_->GetComponent<PrimitiveRendererComponent>()) {
            renderer->GetMaterial().color = kBodyColor;
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
    // 鎖で倒す
    outCustom.push_back(stunTimer_);
    outCustom.push_back(staggerTimer_);
    outCustom.push_back(hitTimer_);
    outCustom.push_back(tripTimer_);
    outCustom.push_back(static_cast<float>(boundUnits_));
}

void GuardBlock::RestoreReplayState(const std::vector<float>& custom) {
    if (custom.size() < 5) return;
    state_ = static_cast<State>(static_cast<int>(custom[0]));
    direction_ = static_cast<int>(custom[1]);
    currentFacing_ = custom[2];
    alertGauge_ = custom[3];
    waitTimer_ = custom[4];
    if (custom.size() >= 10) {
        stunTimer_ = custom[5];
        staggerTimer_ = custom[6];
        hitTimer_ = custom[7];
        tripTimer_ = custom[8];
        boundUnits_ = static_cast<int>(custom[9]);
    } else {
        stunTimer_ = staggerTimer_ = hitTimer_ = tripTimer_ = 0.0f;
        boundUnits_ = 0;
    }

    // 位置は共通処理側で復元済みなので、速度の計算基準だけ合わせておく
    if (gameObject_) {
        if (auto* tc = gameObject_->GetComponent<TransformComponent>()) {
            prevPosition_ = tc->GetPosition();
        }
    }
    deltaPosition_ = {0.0f, 0.0f, 0.0f};
    currentVelocity_ = {0.0f, 0.0f, 0.0f};
}
