#include "ChainSpinAction.h"
#include "Game2D/Chain/Chain2D.h"
#include "Game2D/Player/Player2D.h"
#include "Game2D/MapChip2D.h"
#include "Game2D/Blocks/BaseBlock.h"
#include "Core/Utility/UtilityFunctions.h"
#include "GameObject/PrimitiveObject.h"
#include "Resource/Primitive/PrimitiveManager.h"
#include "Renderer/DirectXCommon/DirectXCommon.h"
#include "Resource/Audio/AudioManager.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace {
    constexpr float kPi = std::numbers::pi_v<float>;
    constexpr float kDeg = 180.0f / kPi;
    // 半径の下限（鎖が極端に短い時）
    constexpr float kMinSpinRadius = 0.3f;
    // 棒（手→重り）がブロックに入っていないかを調べる間隔と開始距離
    constexpr float kRodProbeStep = 0.25f;
    constexpr float kRodProbeStart = 0.5f;
    // 発射準備完了とみなす上限に対する割合（お宝を明るくする合図）
    constexpr float kLaunchReadyRatio = 0.8f;
    // 持ってからこの秒数は A/D を投げ入力にしない（歩きながら Q を押した瞬間に投げてしまうのを防ぐ）
    constexpr float kThrowGrace = 0.15f;
    // やめた後、再び持てるまで（Q 連打の防止）
    constexpr float kHoldBlock = 0.2f;
    // 板以外で投げた後、宝石が飛んでいる間は拾い直せないようにする待ち時間（秒）
    constexpr float kGroundThrowBlock = 0.45f;
    // 矢じり：円錐の半径（高さはその 2 倍）、体の中心からの隙間、勢い最大の時の長さ倍率
    constexpr float kArrowSize = 0.18f;
    constexpr float kArrowGap = 0.25f;
    constexpr float kArrowMaxStretch = 2.2f;
    // 残像：数、球の半径、円周上の間隔（rad。宝石の後ろへ並ぶ）
    constexpr int kTrailDots = 4;
    constexpr float kTrailDotRadius = 0.11f;
    constexpr float kTrailStep = 0.13f;
    // 宝石の光り始め：窓の外側、窓の幅 × これ の所から光り始めて窓の縁で最大（離す前に「来る」と分かる）
    constexpr float kGlowLeadRatio = 2.0f;

    float WrapAngle(float a) {
        while (a > kPi) a -= 2.0f * kPi;
        while (a < -kPi) a += 2.0f * kPi;
        return a;
    }
    float EaseOut(float u) { return 1.0f - (1.0f - u) * (1.0f - u); }

    // 点がソリッドな地形の中か（静止ブロックはチップ単位、動くブロックはAABB。片方向床などは素通り）
    bool PointBlocked(MapChip2D* map, float x, float y) {
        int cx = map->WorldToChipX(x);
        int cy = map->WorldToChipY(y);
        if (auto* block = map->GetBlock(cx, cy)) {
            if (block->IsSolid() && !block->IsDestroyed() && !block->IsMoving()) {
                return true;
            }
        } else if (map->GetChipType(cx, cy) == MapChip2D::ChipType::kBlock) {
            return true;
        }
        for (const auto& blockPtr : map->GetUpdateBlocks()) {
            if (!blockPtr || blockPtr->IsDestroyed() || !blockPtr->IsMoving() || !blockPtr->IsSolid()) {
                continue;
            }
            AABB2D box = blockPtr->GetAABB();
            if (x >= box.left && x <= box.right && y >= box.bottom && y <= box.top) {
                return true;
            }
        }
        return false;
    }
}

void ChainSpinAction::Initialize(const ChainParams& params) {
    params_ = params;
    state_ = State::kIdle;
    toggleEdge_ = false;
    launchHeld_ = launchHeldPrev_ = false;
    aiming_ = false;
    aimTimer_ = 0.0f;
    holdBlockTimer_ = 0.0f;
    throwLockTime_ = -1.0f;
    swingInput_ = 0.0f;
    spinAllowed_ = false;
    theta_ = 0.0f;
    omega_ = 0.0f;
    radius_ = 0.0f;
    throwOutTime_ = 0.0f;
    holdTime_ = 0.0f;
    effMass_ = 1.0f;
    launchCap_ = 0.0f;
    cooldownTimer_ = 0.0f;
    spinAccumAngle_ = 0.0f;
    lastLaunchSpeed_ = 0.0f;
    lastLaunchDir_ = { 0.0f, 0.0f, 0.0f };
    lastBrokeByTerrain_ = false;
    lastLaunchJust_ = false;
    currentSlowScale_ = 1.0f;
    arrowVisible_ = false;
    trailVisible_ = 0;
}

void ChainSpinAction::ResetInputState() {
    toggleEdge_ = false;
    launchHeld_ = launchHeldPrev_ = false;
    aiming_ = false;
    aimTimer_ = 0.0f;
    holdBlockTimer_ = 0.0f;
    throwLockTime_ = -1.0f;
    swingInput_ = 0.0f;
    spinAllowed_ = false;
    currentSlowScale_ = 1.0f;
    // 振り子の状態も入力から決まるので一緒に戻す（前回のプレイ/再生ループの値が残ると0フレーム目からずれる）
    theta_ = 0.0f;
    omega_ = 0.0f;
    radius_ = 0.0f;
    throwOutTime_ = 0.0f;
    holdTime_ = 0.0f;
    spinAccumAngle_ = 0.0f;
    arrowVisible_ = false;
    trailVisible_ = 0;
}

float ChainSpinAction::GetCurrentThrowSpeed() const {
    // 遠心力：接線速度 = |ω| × r（自分で振って得た勢いそのもの）
    return std::fabs(omega_) * radius_ * params_.weightThrowScale_;
}

bool ChainSpinAction::IsLaunchReady() const {
    if (state_ != State::kStance || launchCap_ <= 0.0f) {
        return false;
    }
    return GetCurrentThrowSpeed() * params_.pullTransfer_ >= launchCap_ * kLaunchReadyRatio;
}

float ChainSpinAction::EffectiveMass(Player2D* player) const {
    float units = player ? static_cast<float>((std::max)(0, player->GetChainLength())) : 0.0f;
    return (std::max)(0.1f, params_.treasureMass_ + units * params_.chainMassPerUnit_);
}

float ChainSpinAction::FullRadius(Chain2D* chain) const {
    float radius = (std::min)(params_.spinRadiusMax_, chain->GetTotalLength() * params_.spinRadiusRatio_);
    return (std::max)(radius, kMinSpinRadius);
}

Vector3 ChainSpinAction::TangentDirection() const {
    // 真下=0 の角度で位置は (sinθ, -cosθ)。接線は (cosθ, sinθ)、向きは角速度の符号
    float sign = (omega_ >= 0.0f) ? 1.0f : -1.0f;
    return { std::cos(theta_) * sign, std::sin(theta_) * sign, 0.0f };
}

void ChainSpinAction::UpdateSpinTarget(const Vector3& socketWorld) {
    if (state_ == State::kHold) {
        // 宝石を両手で手前（胸の前）に抱えて持つ（構えポーズに合わせる。チームメイトの変更）
        spinTarget_ = { socketWorld.x, socketWorld.y + 0.05f, 0.0f };
        return;
    }
    // 真下=0 の角度なので位置は (sinθ, -cosθ)。鎖全体はソケット→ここの直線上に拘束される
    spinTarget_ = { socketWorld.x + std::sin(theta_) * radius_,
                    socketWorld.y - std::cos(theta_) * radius_,
                    0.0f };
}

bool ChainSpinAction::IsSlowRegion() const {
    float localX = std::sin(theta_);
    if (omega_ > 0.01f) {
        // 右回転（Dで加速）：プレイヤーから見て右側（X > 0）の間だけスロー
        return localX > 0.0f;
    } else if (omega_ < -0.01f) {
        // 左回転（Aで加速）：プレイヤーから見て左側（X < 0）の間だけスロー
        return localX < 0.0f;
    }
    return true;
}

bool ChainSpinAction::IsSlowActive() const {
    // スペースキーを押して狙い（スロー）モード中はアクティブ。離した瞬間にfalseになり滑らかに解除される
    return (state_ == State::kStance && aiming_);
}

bool ChainSpinAction::IsRodBlocked(MapChip2D* map, const Vector3& socketWorld, float theta, float radius, float endRadius) const {
    if (!map) {
        return false;
    }
    float dx = std::sin(theta);
    float dy = -std::cos(theta);
    float length = radius + endRadius;
    for (float s = kRodProbeStart; s < length; s += kRodProbeStep) {
        if (PointBlocked(map, socketWorld.x + dx * s, socketWorld.y + dy * s)) {
            return true;
        }
    }
    return PointBlocked(map, socketWorld.x + dx * length, socketWorld.y + dy * length);
}

// ---------------------------------------------------------------------------
// 発射のアシスト
// ---------------------------------------------------------------------------

float ChainSpinAction::TangentAngleDeg() const {
    Vector3 d = TangentDirection();
    // 水平から上向きを正（左右は問わない）。下向きなら負
    return std::atan2(d.y, std::fabs(d.x)) * kDeg;
}

float ChainSpinAction::MaxReachableAngleDeg() const {
    // エネルギー保存：½ω²r² = g r (cosθ_max の差) → cosθ_max = cosθ − ω² r / (2g)
    float g = std::fabs(params_.gravity_);
    if (radius_ <= 0.0f || g <= 0.0f) return 0.0f;
    float c = std::cos(theta_) - (omega_ * omega_ * radius_) / (2.0f * g);
    if (c <= -1.0f) return 90.0f;              // 一回転できる勢い：どの角度にも届く
    float thetaMax = std::acos(std::clamp(c, -1.0f, 1.0f)); // 真下からの最大振れ角（rad）
    // 折り返し点で接線は水平から (θ_max) の向き。接線の上向き角は最大でこの角（90 度を超えない）
    return (std::min)(90.0f, thetaMax * kDeg);
}

float ChainSpinAction::WindowCenterDeg() const {
    return (std::min)(params_.launchAngleDeg_, MaxReachableAngleDeg());
}

bool ChainSpinAction::IsInJustWindow() const {
    if (state_ != State::kStance) return false;
    Vector3 d = TangentDirection();
    if (d.y <= 0.0f) return false; // 下向きの時は窓に入らない
    return std::fabs(TangentAngleDeg() - WindowCenterDeg()) <= params_.justWindowDeg_;
}

float ChainSpinAction::GetTimingGlow() const {
    if (state_ != State::kStance) return 0.0f;
    if (TangentDirection().y <= 0.0f) return 0.0f;
    const float window = (std::max)(params_.justWindowDeg_, 1.0f);
    const float dist = std::fabs(TangentAngleDeg() - WindowCenterDeg());
    const float lead = window * kGlowLeadRatio;
    float glow = 0.0f;
    if (dist <= window) glow = 1.0f;
    else if (dist < window + lead) glow = 1.0f - (dist - window) / lead;
    if (!IsLaunchReady()) glow *= 0.4f; // 勢い不足の間は弱く（離しても遠くへ飛ばない）
    return glow;
}

Vector3 ChainSpinAction::ClampToCone(const Vector3& dir) const {
    float sx = (dir.x >= 0.0f) ? 1.0f : -1.0f;
    float ang = std::atan2(dir.y, std::fabs(dir.x)) * kDeg;
    ang = std::clamp(ang, params_.coneMinDeg_, params_.coneMaxDeg_);
    float r = ang / kDeg;
    return { std::cos(r) * sx, std::sin(r), 0.0f };
}

Vector3 ChainSpinAction::PredictLaunchVelocity(bool just) const {
    float speed = std::clamp(GetCurrentThrowSpeed() * params_.pullTransfer_, 0.0f, launchCap_);
    Vector3 dir;
    if (just) {
        // ジャスト：向きを launchAngleDeg_（届く最大角）にそろえ、少しボーナス（上限は超えない）
        float sx = (TangentDirection().x >= 0.0f) ? 1.0f : -1.0f;
        float r = WindowCenterDeg() / kDeg;
        dir = { std::cos(r) * sx, std::sin(r), 0.0f };
        speed = std::clamp(speed * params_.justBonus_, 0.0f, launchCap_);
    } else {
        dir = ClampToCone(TangentDirection());
    }
    return { dir.x * speed, dir.y * speed, 0.0f };
}

// ---------------------------------------------------------------------------
// 更新
// ---------------------------------------------------------------------------

void ChainSpinAction::Update(float dt, MapChip2D* map, Player2D* player, Chain2D* chain, const Vector3& socketWorld) {
    bool toggle = toggleEdge_;
    toggleEdge_ = false;
    const bool held = launchHeld_;
    const bool heldPrev = launchHeldPrev_;
    launchHeldPrev_ = held;
    if (holdBlockTimer_ > 0.0f) holdBlockTimer_ -= dt;
    arrowVisible_ = false;
    trailVisible_ = 0;

    if (!player || !chain) {
        return;
    }
    if (player->IsDead() || player->IsGoal()) {
        if (state_ != State::kIdle) {
            Cancel(player, chain);
        }
        if (throwLockTime_ >= 0.0f) {
            player->SetFaceDirection(0.0f);
            throwLockTime_ = -1.0f;
        }
        return;
    }

    // 板以外で投げた直後：投げた勢いのまま歩き出さないよう、少しの間だけ動けなくする
    // 投げるのに押した A/D を押しっぱなしの間は解除しない（離せば歩ける。長すぎないよう上限あり）
    if (throwLockTime_ >= 0.0f) {
        throwLockTime_ += dt;
        const bool stillHolding = (throwLockDir_ > 0.0f) ? (swingInput_ > 0.5f) : (swingInput_ < -0.5f);
        const float maxLock = params_.groundThrowRecover_ * 4.0f;
        if (throwLockTime_ < params_.groundThrowRecover_ || (stillHolding && throwLockTime_ < maxLock)) {
            player->SetActionInputModifier(0.0f, true);
            player->SetFaceDirection(throwLockDir_); // 動かないので、向きだけ投げた方へ
        } else {
            player->SetActionInputModifier(1.0f, false);
            player->SetFaceDirection(0.0f);
            throwLockTime_ = -1.0f;
        }
    }

    switch (state_) {
    case State::kIdle:
        // Q：地上ならどこでも宝石を持てる（投げて振り子に入れるのは回せる場所だけ）
        if (toggle && player->IsOnGround() && holdBlockTimer_ <= 0.0f) {
            StartHold(player, chain, socketWorld);
        }
        break;

    case State::kHold: {
        // 持っている間：足場を離れたら落とす。Q でも落とす（SPACE は ChainManager が Cancel してからジャンプに渡す）
        if (!player->IsOnGround() || toggle) {
            Cancel(player, chain);
            holdBlockTimer_ = kHoldBlock;
            break;
        }
        player->SetActionInputModifier(params_.spinMoveFactor_, true);
        radius_ = params_.holdOffset_;
        theta_ = kPi;
        UpdateSpinTarget(socketWorld);

        // A/D で押した方向へ投げる。持った直後は投げない
        // 木の板の上なら振り子に入り、それ以外の床なら回さずにその場から放る（警備員に当てやすくするため）
        holdTime_ += dt;
        if (holdTime_ >= kThrowGrace) {
            float dir = (swingInput_ > 0.5f) ? 1.0f : ((swingInput_ < -0.5f) ? -1.0f : 0.0f);
            if (dir != 0.0f) {
                if (IsSpinAllowed()) {
                    StartThrow(dir, socketWorld);
                } else {
                    ThrowFromGround(dir, dt, player, chain);
                }
            }
        }
        break;
    }

    case State::kStance: {
        if (!player->IsOnGround()) {
            Break(dt, player, chain, socketWorld);
            break;
        }
        // Q：やめる（鎖は勢いのまま物理へ）
        if (toggle) {
            Break(dt, player, chain, socketWorld);
            holdBlockTimer_ = kHoldBlock;
            break;
        }

        // SPACE：押した瞬間から狙い（スロー）モード。キーを離した瞬間に発射（押し続けている間は何回転しても解除されない）
        if (held && !heldPrev) {
            aiming_ = true;
            aimTimer_ = 0.0f;
        }
        if (aiming_) {
            aimTimer_ += dt;
            if (!held) {
                Launch(dt, player, chain, socketWorld, IsInJustWindow());
                break;
            }
        }

        // 構え中は移動不可（A/Dは振りに使う）。ジャンプも無効（SPACE は発射に使う）
        player->SetActionInputModifier(params_.spinMoveFactor_, true);

        // 半周スローモーション（動的バレットタイム方式）：
        // 右回転中はプレイヤーの右側（X > 0）のみスロー、左回転中は左側（X < 0）のみスロー
        // 回転が高速（omega大）な場合でも見かけの回転速度が aimTargetOmega_（約1.8 rad/s）程度になるよう、
        // スロー倍率を動的に深める（ただし通常の aimSlow_ よりは速くならない）
        const bool inSlowRegion = aiming_ && IsSlowRegion();
        float targetSlow = 1.0f;
        if (inSlowRegion) {
            const float absOmega = std::fabs(omega_);
            if (absOmega > 0.01f) {
                targetSlow = (std::min)(params_.aimSlow_, params_.aimTargetOmega_ / absOmega);
            } else {
                targetSlow = params_.aimSlow_;
            }
        }
        // スローへ入るときは素早く(12.0f/s)、スローから戻るときは「もっとゆっくり滑らかに」(1.2f/s) 補間
        const float transitionSpeed = (targetSlow > currentSlowScale_) ? 1.2f : 12.0f;
        currentSlowScale_ += (targetSlow - currentSlowScale_) * std::clamp(transitionSpeed * dt, 0.0f, 1.0f);

        const float simDt = dt * currentSlowScale_;

        float full = FullRadius(chain);
        throwOutTime_ += simDt;
        float ramp = std::clamp(throwOutTime_ / (std::max)(0.001f, params_.throwOutTime_), 0.0f, 1.0f);
        radius_ = params_.holdOffset_ + (full - params_.holdOffset_) * EaseOut(ramp);
        radius_ = (std::max)(radius_, kMinSpinRadius);

        effMass_ = EffectiveMass(player);

        // D長押しで右回転に徐々に加速、A長押しで左回転に徐々に加速
        // 右回転中にAで急ブレーキ、左回転中にDで急ブレーキ
        // A/Dが離れたら自然減速（スローモーション区間では減衰率も時間の進みsimDtに合わせてスケーリング）
        const float accel = params_.spinAccel_;
        const float brake = params_.spinBrake_;
        const float maxOmega = params_.maxSpinOmega_;

        if (swingInput_ > 0.5f) {
            // D が押されている：右回転（下から右へ駆け上がる回転: omega_ > 0）
            if (omega_ < -0.1f) {
                // 現在は左回転（omega < 0）なので急ブレーキ！
                omega_ += brake * simDt;
                if (omega_ > 0.0f) {
                    omega_ = 0.0f;
                }
            } else {
                // 右回転方向に徐々に加速
                omega_ += accel * simDt;
                if (omega_ > maxOmega) {
                    omega_ = maxOmega;
                }
            }
        } else if (swingInput_ < -0.5f) {
            // A が押されている：左回転（下から左へ駆け上がる回転: omega_ < 0）
            if (omega_ > 0.1f) {
                // 現在は右回転（omega > 0）なので急ブレーキ！
                omega_ -= brake * simDt;
                if (omega_ < 0.0f) {
                    omega_ = 0.0f;
                }
            } else {
                // 左回転方向に徐々に加速
                omega_ -= accel * simDt;
                if (omega_ < -maxOmega) {
                    omega_ = -maxOmega;
                }
            }
        } else {
            // A/D が離れている：自然減衰
            // simDt を用いているため、スローモーション区間では減衰率も時間の進みと連動
            omega_ *= (std::max)(0.0f, 1.0f - params_.swingDamping_ * simDt);
            if (std::fabs(omega_) < 0.05f) {
                omega_ = 0.0f;
            }
        }

        const float dTheta = omega_ * simDt;
        theta_ = WrapAngle(theta_ + dTheta);

        // チェーンが1回転（360度 = 2π rad）するたびに SE を鳴らす
        if (dTheta * spinAccumAngle_ < 0.0f) {
            // 振り子のように回転方向が逆転した場合は累積をリセット
            spinAccumAngle_ = 0.0f;
        }
        spinAccumAngle_ += dTheta;
        while (std::fabs(spinAccumAngle_) >= 2.0f * kPi) {
            AudioManager::Play("resources/Sound/10Dyas/SE/TurnChain.mp3", 0.75f);
            if (spinAccumAngle_ > 0.0f) {
                spinAccumAngle_ -= 2.0f * kPi;
            } else {
                spinAccumAngle_ += 2.0f * kPi;
            }
        }

        if (IsRodBlocked(map, socketWorld, theta_, radius_, chain->GetEndWeight().radius)) {
            Break(dt, player, chain, socketWorld);
            break;
        }
        UpdateSpinTarget(socketWorld);

        // 矢じり（スロー中、今離したら飛ぶ向き）と残像（進んでいる向き）
        UpdateVisuals(player, socketWorld);
        break;
    }

    case State::kCooldown:
        cooldownTimer_ -= dt;
        if (cooldownTimer_ <= 0.0f || player->IsOnGround()) {
            state_ = State::kIdle;
        }
        break;
    }
}

void ChainSpinAction::StartHold(Player2D* player, Chain2D* chain, const Vector3& socketWorld) {
    state_ = State::kHold;
    omega_ = 0.0f;
    theta_ = kPi;
    throwOutTime_ = 0.0f;
    holdTime_ = 0.0f;
    aiming_ = false;
    aimTimer_ = 0.0f;
    lastBrokeByTerrain_ = false;
    spinAccumAngle_ = 0.0f;
    currentSlowScale_ = 1.0f;

    launchCap_ = player->GetParams().jumpPower_ * params_.launchMaxJumpRatio_;
    effMass_ = EffectiveMass(player);

    radius_ = params_.holdOffset_;
    UpdateSpinTarget(socketWorld);
    chain->SetRigidLineTarget(&spinTarget_);
    player->SetActionInputModifier(params_.spinMoveFactor_, true);
}

void ChainSpinAction::StartThrow(float dirSign, const Vector3& socketWorld) {
    state_ = State::kStance;
    // dirSign > 0 (D) なら右回転（omega > 0）、dirSign < 0 (A) なら左回転（omega < 0）
    omega_ = dirSign * params_.throwOmega_;
    theta_ = 0.0f;
    throwOutTime_ = 0.0f;
    radius_ = params_.holdOffset_;
    aiming_ = false;
    aimTimer_ = 0.0f;
    spinAccumAngle_ = 0.0f;
    // 投げた瞬間に SPACE を押していても、押した縁を作らない（離した時に飛ばないように）
    launchHeldPrev_ = launchHeld_;
    UpdateSpinTarget(socketWorld);
    Log("ChainSpinAction: throw dir=" + std::to_string(dirSign) + "\n");
}

void ChainSpinAction::ThrowFromGround(float dirSign, float dt, Player2D* player, Chain2D* chain) {
    if (!chain) {
        return;
    }
    // 押した方向へ、少し上向きに放る。鎖は繋がったままなので伸び切ると戻ってくる
    Vector3 dir = { dirSign, params_.groundThrowUp_, 0.0f };
    float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
    if (len > 1e-6f) { dir.x /= len; dir.y /= len; }
    chain->ThrowWeight({ dir.x * params_.groundThrowSpeed_, dir.y * params_.groundThrowSpeed_, 0.0f }, dt);

    // 投げた直後は動けないままにする（A/D を押しっぱなしで投げるので、そのまま歩き出さないように）
    throwLockTime_ = 0.0f;
    throwLockDir_ = dirSign;
    if (player) {
        player->SetFaceDirection(dirSign); // 投げた方を向く
    }
    if (player && params_.groundThrowRecover_ <= 0.0f) {
        player->SetActionInputModifier(1.0f, false);
        throwLockTime_ = -1.0f;
    }
    state_ = State::kIdle;
    omega_ = 0.0f;
    theta_ = 0.0f;
    radius_ = 0.0f;
    throwOutTime_ = 0.0f;
    holdTime_ = 0.0f;
    aiming_ = false;
    aimTimer_ = 0.0f;
    arrowVisible_ = false;
    trailVisible_ = 0;
    holdBlockTimer_ = kGroundThrowBlock; // 投げた直後に Q で拾い直せないようにする
    Log("ChainSpinAction: ground throw dir=" + std::to_string(dirSign) + "\n");
}

void ChainSpinAction::Launch(float dt, Player2D* player, Chain2D* chain, const Vector3& socketWorld, bool just) {
    float throwSpeed = GetCurrentThrowSpeed();
    // 鎖全体に振りの回転速度（スロー前の勢い）を与えて物理に戻す。宝石はその勢いのまま飛び、プレイヤーを引っ張る
    chain->ReleaseRigidLine(socketWorld, omega_, params_.weightThrowScale_, dt);

    Vector3 v = PredictLaunchVelocity(just);
    float speed = std::sqrt(v.x * v.x + v.y * v.y);
    Vector3 dir = (speed > 1e-6f) ? Vector3{ v.x / speed, v.y / speed, 0.0f } : Vector3{ 0.0f, 1.0f, 0.0f };
    // 念のため最低限の上向きを確保（コーンで既に上向きだが、パラメータで潰された時の保険）
    if (dir.y < params_.launchMinUpward_) {
        dir.y = params_.launchMinUpward_;
        float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
        if (len > 1e-6f) { dir.x /= len; dir.y /= len; }
    }
    player->Launch({ dir.x * speed, dir.y * speed, 0.0f });
    player->TriggerSpinFlip(omega_);
    player->SetActionInputModifier(1.0f, false);

    lastLaunchSpeed_ = speed;
    lastLaunchDir_ = dir;
    lastLaunchJust_ = just;
    lastBrokeByTerrain_ = false;
    aiming_ = false;
    aimTimer_ = 0.0f;
    spinAccumAngle_ = 0.0f;
    currentSlowScale_ = 1.0f;
    state_ = State::kCooldown;
    cooldownTimer_ = params_.spinCooldown_;
    Log("ChainSpinAction: Launch speed=" + std::to_string(speed) + " / cap " + std::to_string(launchCap_) +
        " (throw " + std::to_string(throwSpeed) + ")" + (just ? " JUST" : "") +
        " dir=(" + std::to_string(dir.x) + ", " + std::to_string(dir.y) + ")\n");
}

void ChainSpinAction::Break(float dt, Player2D* player, Chain2D* chain, const Vector3& socketWorld) {
    chain->ReleaseRigidLine(socketWorld, omega_, params_.weightThrowScale_, dt);
    player->SetActionInputModifier(1.0f, false);
    lastBrokeByTerrain_ = true;
    aiming_ = false;
    aimTimer_ = 0.0f;
    spinAccumAngle_ = 0.0f;
    currentSlowScale_ = 1.0f;
    state_ = State::kCooldown;
    cooldownTimer_ = params_.spinCooldown_;
    Log("ChainSpinAction: stance broken (terrain/airborne/cancel) omega=" + std::to_string(omega_) + "\n");
}

void ChainSpinAction::Cancel(Player2D* player, Chain2D* chain) {
    if ((state_ == State::kHold || state_ == State::kStance) && chain) {
        chain->SetRigidLineTarget(nullptr);
    }
    if (player) {
        player->SetActionInputModifier(1.0f, false);
    }
    state_ = State::kIdle;
    omega_ = 0.0f;
    throwOutTime_ = 0.0f;
    cooldownTimer_ = 0.0f;
    aiming_ = false;
    aimTimer_ = 0.0f;
    spinAccumAngle_ = 0.0f;
    currentSlowScale_ = 1.0f;
    arrowVisible_ = false;
    trailVisible_ = 0;
}

// ---------------------------------------------------------------------------
// 矢じりと残像
// ---------------------------------------------------------------------------

void ChainSpinAction::EnsureVisuals() {
    ID3D12Device* device = DirectXCommon::GetInstance()->GetDevice();
    if (!device) return;
    if (!arrow_) {
        // 矢じり（円錐。軸は +Y なので、向きに合わせて Z 回転で倒す）
        Primitive* cone = PrimitiveManager::GetInstance()->GetPrimitive(PrimitiveType::Cone, kArrowSize, 12);
        if (cone) {
            arrow_ = std::make_unique<PrimitiveObject>();
            arrow_->Initialize(device, cone);
            arrow_->SetName("LaunchArrow");
            arrow_->GetMaterial().lightingType = 0;
            arrow_->GetMaterial().enableEnvironmentMap = 0;
            arrow_->SetIsBillboard(false);
            arrow_->SetIsDoubleSided(true);
        }
    }
    if (trailDots_.empty()) {
        Primitive* sphere = PrimitiveManager::GetInstance()->GetPrimitive(PrimitiveType::Sphere, kTrailDotRadius, 8);
        if (sphere) {
            for (int i = 0; i < kTrailDots; ++i) {
                auto dot = std::make_unique<PrimitiveObject>();
                dot->Initialize(device, sphere);
                dot->SetName("GemTrailDot");
                dot->GetMaterial().lightingType = 0;
                dot->GetMaterial().enableEnvironmentMap = 0;
                trailDots_.push_back(std::move(dot));
            }
        }
    }
}

void ChainSpinAction::UpdateVisuals(Player2D* player, const Vector3& socketWorld) {
    EnsureVisuals();
    if (radius_ <= 0.0f || !player) return;

    const bool just = IsInJustWindow();
    const bool ready = IsLaunchReady();
    const Vector4 gold = { 1.0f, 0.85f, 0.25f, 0.95f };
    const Vector4 dim = { 0.75f, 0.7f, 0.45f, 0.55f };
    const Vector4 bright = { 1.0f, 0.95f, 0.6f, 1.0f };

    // ---- 矢じり：エイム中、プレイヤーの体から「今離したら自分が飛ぶ向き」を指す ----
    // 飛ぶのはプレイヤーなので、宝石ではなく体から出す。勢いが強いほど長く伸びる
    arrowVisible_ = false;
    if (arrow_ && aiming_) {
        // 矢印の向き：接線方向（TangentDirection）を直接使用
        // チェーンの剛体回転速度ベクトルと完全に一致し、チェーンの回転と1対1で360度滑らかに完全連動する
        Vector3 dir = TangentDirection();

        const float speed = std::clamp(GetCurrentThrowSpeed() * params_.pullTransfer_, 0.0f, launchCap_);
        // 長さ：勢い（上限に対する割合）で伸ばす。円錐の高さは kArrowSize × 2
        const float ratio = (launchCap_ > 0.0f) ? std::clamp(speed / launchCap_, 0.0f, 1.0f) : 0.0f;
        const float stretch = 1.0f + (kArrowMaxStretch - 1.0f) * ratio;
        const float length = kArrowSize * 2.0f * stretch;
        // 体の中心から、体の外側（半身 + 隙間）に根元を置く
        const Vector3 body = player->GetPosition();
        const float root = player->GetParams().halfHeight_ + kArrowGap;
        const float center = root + length * 0.5f;
        Vector3 pos = { body.x + dir.x * center, body.y + dir.y * center, -0.3f };
        arrow_->SetTranslation(pos);
        // 円錐の先端は +Y。向き（右 = 0 度）へ倒すには Z 回転 = 角度 − 90 度
        arrow_->SetRotation({ 0.0f, 0.0f, std::atan2(dir.y, dir.x) - kPi * 0.5f });
        // 勢い不足は細く暗く、飛べるなら金、ジャスト窓内は白っぽく太く強く発光
        float w = ready ? 1.0f : 0.7f;
        Vector4 color = ready ? gold : dim;
        if (just && ready) {
            w = 1.4f;
            color = bright;
        }
        arrow_->SetScale({ w, stretch, w });
        arrow_->GetMaterial().color = color;
        arrow_->Update();
        arrowVisible_ = true;
    }

    // ---- 残像：円周上、進んできた側に並ぶ薄い球（彗星の尾）。尾の反対側が進行方向 ----
    trailVisible_ = 0;
    if (!trailDots_.empty() && std::fabs(omega_) > 0.05f) {
        const float back = (omega_ >= 0.0f) ? -1.0f : 1.0f; // 進行方向と逆へ
        const int n = static_cast<int>(trailDots_.size());
        for (int i = 0; i < n; ++i) {
            const float th = theta_ + back * kTrailStep * static_cast<float>(i + 1);
            Vector3 q = { socketWorld.x + std::sin(th) * radius_, socketWorld.y - std::cos(th) * radius_, -0.15f };
            const float k = 1.0f - static_cast<float>(i) / static_cast<float>(n); // 宝石に近いほど 1
            const float s = 0.45f + 0.55f * k;
            auto& dot = trailDots_[i];
            dot->SetTranslation(q);
            dot->SetScale({ s, s, s });
            Vector4 c = ready ? gold : dim;
            c.w *= 0.15f + 0.45f * k;
            dot->GetMaterial().color = c;
            dot->Update();
            ++trailVisible_;
        }
    }
}

void ChainSpinAction::Draw() {
    if (state_ != State::kStance) return;
    for (int i = 0; i < trailVisible_ && i < static_cast<int>(trailDots_.size()); ++i) {
        trailDots_[i]->Draw();
    }
    if (arrowVisible_ && arrow_) {
        arrow_->Draw();
    }
}

void ChainSpinAction::DrawImGui() {
#ifdef USE_IMGUI
    const char* stateNames[] = { "Idle", "Hold", "Stance", "Cooldown" };
    const bool isSlow = aiming_ && IsSlowRegion();
    ImGui::Text("Spin: %s%s  theta %.2f  omega %.2f  radius %.2f  mass %.2f  swing %+.0f%s",
                stateNames[static_cast<int>(state_)], isSlow ? " (slow)" : (aiming_ ? " (aim/fast)" : ""), theta_, omega_, radius_, effMass_, swingInput_,
                lastBrokeByTerrain_ ? "  [last: broken]" : "");
    ImGui::Text("Throw now: %.1f u/s -> fly %.1f / cap %.1f %s  (last %.1f%s, dir %.2f, %.2f)",
                GetCurrentThrowSpeed(), GetCurrentThrowSpeed() * params_.pullTransfer_, launchCap_,
                IsLaunchReady() ? "[ready]" : "[weak]", lastLaunchSpeed_, lastLaunchJust_ ? " JUST" : "",
                lastLaunchDir_.x, lastLaunchDir_.y);
    ImGui::Text("Window: center %.1f deg (reach %.1f)  tangent %.1f deg  %s  aim %.2f s",
                WindowCenterDeg(), MaxReachableAngleDeg(), TangentAngleDeg(),
                IsInJustWindow() ? "[JUST]" : "", aimTimer_);
#endif
}
