#include "GuardBlock.h"
#include "Editor/Replay/ReplayManager.h"
#include "Game2D/MapChip2D.h"
#include "Game2D/Player/Player2D.h"
#include "GameObject/Object3D.h"
#include "Resource/Model/ModelManager.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <DirectXMath.h>
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
    boxPrimitive_ = boxPrimitive;

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

    // 懐中電灯本体パーツ（小さな直方体）
    flashlightBodyObj_ = std::make_unique<GameObject>();
    flashlightBodyObj_->Initialize();
    flashlightBodyObj_->SetName("GuardFlashlightBody");
    auto* flbTransform = flashlightBodyObj_->AddComponent<TransformComponent>();
    flbTransform->SetPosition({worldX, worldY, 0.0f});
    flbTransform->SetScale({0.35f, 0.15f, 0.15f});
    auto* flbRenderer = flashlightBodyObj_->AddComponent<PrimitiveRendererComponent>();
    flbRenderer->Initialize(device, boxPrimitive);
    flbRenderer->GetMaterial().color = {0.15f, 0.15f, 0.18f, 1.0f}; // メタリックダークグレー
    flbRenderer->GetMaterial().lightingType = 1;

    // 懐中電灯レンズ部（先端の発光パーツ）
    flashlightLensObj_ = std::make_unique<GameObject>();
    flashlightLensObj_->Initialize();
    flashlightLensObj_->SetName("GuardFlashlightLens");
    auto* fllTransform = flashlightLensObj_->AddComponent<TransformComponent>();
    fllTransform->SetPosition({worldX, worldY, 0.0f});
    fllTransform->SetScale({0.08f, 0.13f, 0.13f});
    auto* fllRenderer = flashlightLensObj_->AddComponent<PrimitiveRendererComponent>();
    fllRenderer->Initialize(device, boxPrimitive);
    fllRenderer->GetMaterial().color = {1.0f, 1.0f, 0.8f, 1.0f}; // 明るいレンズ色
    fllRenderer->GetMaterial().lightingType = 0; // 自己発光風

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
    // 懐中電灯（スポットライト）
    readF("lightDistance", lightDistance_);
    if (!properties.contains("lightDistance")) {
        lightDistance_ = (std::max)(sightLength_ * 1.6f, 8.0f);
    }
    readF("lightAngleDeg", lightAngleDeg_);
    readF("lightFalloffDeg", lightFalloffDeg_);
    readF("lightIntensity", lightIntensity_);
    readF("lightDecay", lightDecay_);
    // シャドウマッピング
    if (properties.contains("enableShadow") && properties["enableShadow"].is_boolean()) {
        enableShadow_ = properties["enableShadow"];
    }
    readF("shadowBias", shadowBias_);
    readF("shadowIntensity", shadowIntensity_);
}

void GuardBlock::Update() {
    BaseBlock::Update();
    if (!gameObject_) return;

    auto* tc = gameObject_->GetComponent<TransformComponent>();
    if (!tc) return;

    Vector3 currentPos = tc->GetPosition();

#ifdef USE_IMGUI
    // エディタ停止中であっても、懐中電灯パーツの位置合わせや更新は行う
    if (!EditorManager::IsPlaying()) {
        UpdateFlashlight(0.0f);
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

    // 懐中電灯パーツの更新（位置・向き・倒れ同期、発光部カラー更新）
    UpdateFlashlight(dt);

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
    if (flashlightBodyObj_) {
        flashlightBodyObj_->Draw();
    }
    if (flashlightLensObj_) {
        flashlightLensObj_->Draw();
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

namespace {
    // 2点間の線分上にソリッドブロックが存在するか判定（壁遮蔽判定）
    bool CheckLineOfSight(MapChip2D* map, const Vector3& from, const Vector3& to) {
        if (!map) return true;
        float dx = to.x - from.x;
        float dy = to.y - from.y;
        float dist = std::sqrt(dx * dx + dy * dy);
        if (dist < 1e-4f) return true;

        float chipSize = map->GetChipSize();
        if (chipSize <= 0.0f) chipSize = 1.0f;
        // チップサイズの半分以下の刻み幅で細かくサンプリング
        float stepSize = chipSize * 0.35f;
        int steps = static_cast<int>(std::ceil(dist / stepSize));
        if (steps < 2) steps = 2;

        for (int i = 1; i < steps; ++i) {
            float t = static_cast<float>(i) / static_cast<float>(steps);
            float sx = from.x + dx * t;
            float sy = from.y + dy * t;

            int cx = map->WorldToChipX(sx);
            int cy = map->WorldToChipY(sy);

            if (auto* block = map->GetBlock(cx, cy)) {
                // 完全ソリッドかつ非一方向床ブロックであれば光を遮る
                if (block->IsSolid() && !block->IsOneWay()) {
                    return false; // 遮蔽あり
                }
            }
        }
        return true; // 遮蔽なし
    }
}

void GuardBlock::UpdateFlashlight(float dt) {
    (void)dt;
    if (!gameObject_) return;
    auto* tc = gameObject_->GetComponent<TransformComponent>();
    if (!tc) return;

    Vector3 guardPos = tc->GetPosition();
    float facing = currentFacing_;

    // 懐中電灯本体の取り付け位置（前面、腰の高さ、少し手前）
    float handOffsetX = facing * (startWidth_ * 0.5f + 0.15f);
    float handOffsetY = -0.05f;
    float handOffsetZ = -0.3f;

    Vector3 bodyPos = { guardPos.x + handOffsetX, guardPos.y + handOffsetY, guardPos.z + handOffsetZ };

    // 向きと倒れの回転
    float yaw = (1.0f - facing) * 0.5f * kPi;
    Vector3 flRot = { 0.0f, yaw, -tumble_ * kPi * 0.5f };

    if (flashlightBodyObj_) {
        if (auto* flTc = flashlightBodyObj_->GetComponent<TransformComponent>()) {
            flTc->SetPosition(bodyPos);
            flTc->SetRotation(flRot);
            flTc->SetScale({0.35f, 0.15f, 0.15f});
        }
        flashlightBodyObj_->Update();
    }

    if (flashlightLensObj_) {
        // 先端（ボディの先）
        float lensOffsetX = facing * 0.18f;
        Vector3 lensPos = { bodyPos.x + lensOffsetX, bodyPos.y, bodyPos.z };
        if (auto* lensTc = flashlightLensObj_->GetComponent<TransformComponent>()) {
            lensTc->SetPosition(lensPos);
            lensTc->SetRotation(flRot);
            lensTc->SetScale({0.08f, 0.13f, 0.13f});
        }
        if (auto* lensRenderer = flashlightLensObj_->GetComponent<PrimitiveRendererComponent>()) {
            Vector4 lCol = GetCurrentLightColor();
            if (!IsLightActive()) {
                lCol = { 0.15f, 0.15f, 0.15f, 1.0f }; // 消灯
            }
            lensRenderer->GetMaterial().color = lCol;
        }
        flashlightLensObj_->Update();
    }
}

Vector3 GuardBlock::GetLightPosition() const {
    if (!gameObject_) return { startX_, startY_, 0.0f };
    auto* tc = gameObject_->GetComponent<TransformComponent>();
    if (!tc) return { startX_, startY_, 0.0f };

    Vector3 p = tc->GetPosition();
    float facing = currentFacing_;
    float offsetX = facing * (startWidth_ * 0.5f + 0.35f); // 懐中電灯先端
    float offsetY = -0.05f;
    float offsetZ = -0.45f; // 手前から奥に向かって照射し、立体感と背景板への到達を両立
    return { p.x + offsetX, p.y + offsetY, p.z + offsetZ };
}

Vector3 GuardBlock::GetLightDirection() const {
    float facing = currentFacing_;
    float dirX = (facing >= 0.0f) ? 1.0f : -1.0f;
    // 奥（Z+0.57f）と床面（Y-0.06f）に向けることで、背景板と足元の床ブロックを鮮やかに照らし出す
    Vector3 dir = { dirX * 0.82f, -0.06f, 0.57f };
    float len = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
    if (len > 1e-4f) {
        return { dir.x / len, dir.y / len, dir.z / len };
    }
    return { dirX, 0.0f, 0.0f };
}

bool GuardBlock::IsLightActive() const {
    return !IsIncapacitated();
}

Vector4 GuardBlock::GetCurrentLightColor() const {
    if (!IsLightActive()) {
        return { 0.0f, 0.0f, 0.0f, 0.0f };
    }
    if (staggerTimer_ > 0.0f) {
        // よろけ時は点滅
        float blink = std::sin(wobbleTime_ * 40.0f);
        if (blink < 0.0f) return { 0.1f, 0.1f, 0.1f, 1.0f };
    }
    float alertRatio = (maxAlertGauge_ > 0.0f) ? std::clamp(alertGauge_ / maxAlertGauge_, 0.0f, 1.0f) : 0.0f;
    Vector4 col;
    col.x = lightColor_.x + (alertLightColor_.x - lightColor_.x) * alertRatio;
    col.y = lightColor_.y + (alertLightColor_.y - lightColor_.y) * alertRatio;
    col.z = lightColor_.z + (alertLightColor_.z - lightColor_.z) * alertRatio;
    col.w = 1.0f;
    return col;
}

VisionCone GuardBlock::GetVisionCone() const {
    VisionCone cone;
    cone.eyePosition = GetLightPosition();
    cone.forward = GetLightDirection();
    cone.distance = sightLength_;
    cone.halfAngleRad = lightAngleDeg_ * (std::numbers::pi_v<float> / 180.0f);
    return cone;
}

SpotLight GuardBlock::GetSpotLightData() const {
    SpotLight sl{};
    if (!IsLightActive()) {
        sl.enable = 0;
        sl.shadowMapIndex = -1;
        return sl;
    }

    sl.enable = 1;
    sl.color = GetCurrentLightColor();
    sl.position = GetLightPosition();
    sl.direction = GetLightDirection();
    sl.intensity = lightIntensity_;
    sl.distance = lightDistance_;
    sl.decay = lightDecay_;
    sl.cosAngle = std::cos(lightAngleDeg_ * (std::numbers::pi_v<float> / 180.0f));
    sl.cosFalloffStart = std::cos(lightFalloffDeg_ * (std::numbers::pi_v<float> / 180.0f));
    sl.shadowMapIndex = enableShadow_ ? 0 : -1;
    sl.shadowBias = shadowBias_;
    sl.shadowIntensity = shadowIntensity_;

    // シャドウマッピング用viewProjection行列の計算
    Vector3 dir = sl.direction;
    Vector3 eye = sl.position;
    Vector3 target = { eye.x + dir.x, eye.y + dir.y, eye.z + dir.z };
    Vector3 up = { 0.0f, 1.0f, 0.0f };
    if (std::abs(dir.y) > 0.99f) {
        up = { 0.0f, 0.0f, 1.0f };
    }

    DirectX::XMVECTOR eyeV = DirectX::XMVectorSet(eye.x, eye.y, eye.z, 1.0f);
    DirectX::XMVECTOR targetV = DirectX::XMVectorSet(target.x, target.y, target.z, 1.0f);
    DirectX::XMVECTOR upV = DirectX::XMVectorSet(up.x, up.y, up.z, 0.0f);
    DirectX::XMMATRIX viewMat = DirectX::XMMatrixLookAtLH(eyeV, targetV, upV);

    float fovAngle = (lightAngleDeg_ * 2.0f) * (std::numbers::pi_v<float> / 180.0f);
    fovAngle = std::clamp(fovAngle, 0.01f, std::numbers::pi_v<float> * 0.99f);
    DirectX::XMMATRIX projMat = DirectX::XMMatrixPerspectiveFovLH(fovAngle, 1.0f, 0.1f, (sl.distance > 0.5f) ? sl.distance : 20.0f);
    DirectX::XMMATRIX viewProjMat = DirectX::XMMatrixMultiply(viewMat, projMat);
    DirectX::XMStoreFloat4x4(reinterpret_cast<DirectX::XMFLOAT4X4*>(&sl.viewProjection), viewProjMat);

    return sl;
}

AABB2D GuardBlock::GetSightAABB() const {
    if (!IsLightActive()) return {-10000.0f, -10000.0f, -10000.0f, -10000.0f};

    VisionCone cone = GetVisionCone();
    float x0 = cone.eyePosition.x;
    float y0 = cone.eyePosition.y;

    float sinHalf = std::sin(cone.halfAngleRad);
    float dirX = (cone.forward.x >= 0.0f) ? 1.0f : -1.0f;
    float reachX = x0 + dirX * cone.distance;
    float spreadY = cone.distance * sinHalf;

    float left = (std::min)(x0, reachX);
    float right = (std::max)(x0, reachX);
    float top = y0 + spreadY;
    float bottom = y0 - spreadY;

    return { left, top, right, bottom };
}

bool GuardBlock::CheckPlayerInLight(const Vector3& playerPos, float playerRadius, const AABB2D& playerAABB, MapChip2D* map) const {
    if (!IsLightActive()) return false;

    VisionCone cone = GetVisionCone();

    // プレイヤーの主要判定球群（頭部・胸部・足元）
    float centerY = (playerAABB.top + playerAABB.bottom) * 0.5f;
    float r = (playerRadius > 0.05f) ? playerRadius : 0.35f;

    const SphereShape playerSpheres[] = {
        { { playerPos.x, centerY, 0.0f }, r },               // 中心/胸部
        { { playerPos.x, playerAABB.top - r, 0.0f }, r },    // 頭部
        { { playerPos.x, playerAABB.bottom + r, 0.0f }, r }, // 足元
    };

    // 代表点群（四隅や左右端）
    const Vector3 samplePoints[] = {
        { playerAABB.left, centerY, 0.0f },
        { playerAABB.right, centerY, 0.0f },
        { playerAABB.left, playerAABB.top, 0.0f },
        { playerAABB.right, playerAABB.top, 0.0f },
        { playerAABB.left, playerAABB.bottom, 0.0f },
        { playerAABB.right, playerAABB.bottom, 0.0f },
    };

    // 1. 球体交差判定 (IsSphereInVisionCone)
    for (const auto& sphere : playerSpheres) {
        if (IsSphereInVisionCone(sphere, cone)) {
            // 光が届いているか壁遮蔽チェック
            if (CheckLineOfSight(map, cone.eyePosition, sphere.center)) {
                return true;
            }
        }
    }

    // 2. 代表点判定 (IsPointInVisionCone)
    for (const auto& pt : samplePoints) {
        if (IsPointInVisionCone(pt, cone)) {
            if (CheckLineOfSight(map, cone.eyePosition, pt)) {
                return true;
            }
        }
    }

    return false;
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
    currentFacing_ = static_cast<float>(startDirection_);
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
    UpdateFlashlight(0.0f);
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
    UpdateFlashlight(0.0f);
}
