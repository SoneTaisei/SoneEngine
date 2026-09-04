#include "ChainSpinAction.h"
#include "Game2D/Chain/Chain2D.h"
#include "Game2D/Player/Player2D.h"
#include "Game2D/MapChip2D.h"
#include "Game2D/Blocks/BaseBlock.h"
#include "Core/Utility/UtilityFunctions.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace {
    constexpr float kPi = std::numbers::pi_v<float>;
    // 半径の下限（鎖が極端に短い時）
    constexpr float kMinSpinRadius = 0.3f;
    // 棒（手→重り）がブロックに入っていないかを調べる間隔と開始距離
    // （手元付近は壁張り付き時にブロック内へ入り得るので少し離れた所から調べる）
    constexpr float kRodProbeStep = 0.25f;
    constexpr float kRodProbeStart = 0.5f;
    // 発射準備完了とみなす上限に対する割合（お宝を明るくする合図）
    constexpr float kLaunchReadyRatio = 0.8f;

    // 振り子の角度を [-π, π] に保つ（一回転しても sin/cos の精度を落とさない）
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
    keyHeld_ = wasHeld_ = pressEdge_ = releaseEdge_ = false;
    swingInput_ = 0.0f;
    spinAllowed_ = false;
    theta_ = 0.0f;
    omega_ = 0.0f;
    radius_ = 0.0f;
    throwOutTime_ = 0.0f;
    effMass_ = 1.0f;
    launchCap_ = 0.0f;
    cooldownTimer_ = 0.0f;
    lastLaunchSpeed_ = 0.0f;
    lastLaunchDir_ = { 0.0f, 0.0f, 0.0f };
    lastBrokeByTerrain_ = false;
}

void ChainSpinAction::SetKeyHeld(bool held) {
    if (held && !wasHeld_) {
        pressEdge_ = true;
    }
    if (!held && wasHeld_) {
        releaseEdge_ = true;
    }
    wasHeld_ = held;
    keyHeld_ = held;
}

void ChainSpinAction::ResetInputState() {
    keyHeld_ = false;
    wasHeld_ = false;
    pressEdge_ = false;
    releaseEdge_ = false;
    swingInput_ = 0.0f;
    spinAllowed_ = false;
    // 振り子の状態も入力から決まるので一緒に戻す（前回のプレイ/再生ループの値が残ると0フレーム目からずれる）
    theta_ = 0.0f;
    omega_ = 0.0f;
    radius_ = 0.0f;
    throwOutTime_ = 0.0f;
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
    // 宝石の質量 + 鎖の質量（ユニット数 × 1ユニットの質量）。振る力をこれで割る
    float units = player ? static_cast<float>((std::max)(0, player->GetChainLength())) : 0.0f;
    return (std::max)(0.1f, params_.treasureMass_ + units * params_.chainMassPerUnit_);
}

float ChainSpinAction::FullRadius(Chain2D* chain) const {
    // 鎖の実長 × spinRadiusRatio_（1.0 で節間隔ちょうどのピンと張った棒）。長い鎖ほど半径が大きい
    float radius = (std::min)(params_.spinRadiusMax_, chain->GetTotalLength() * params_.spinRadiusRatio_);
    return (std::max)(radius, kMinSpinRadius);
}

Vector3 ChainSpinAction::TangentDirection() const {
    // 真下=0 の角度で位置は (sinθ, -cosθ)。接線は (cosθ, sinθ)、向きは角速度の符号
    float sign = (omega_ >= 0.0f) ? 1.0f : -1.0f;
    return { std::cos(theta_) * sign, std::sin(theta_) * sign, 0.0f };
}

void ChainSpinAction::UpdateSpinTarget(const Vector3& socketWorld) {
    // 真下=0 の角度なので位置は (sinθ, -cosθ)。鎖全体はソケット→ここの直線上に拘束される
    spinTarget_ = { socketWorld.x + std::sin(theta_) * radius_,
                    socketWorld.y - std::cos(theta_) * radius_,
                    0.0f };
}

bool ChainSpinAction::IsRodBlocked(MapChip2D* map, const Vector3& socketWorld, float theta, float radius, float endRadius) const {
    if (!map) {
        return false;
    }
    float dx = std::sin(theta);
    float dy = -std::cos(theta);
    float length = radius + endRadius; // 宝石の外周まで
    for (float s = kRodProbeStart; s < length; s += kRodProbeStep) {
        if (PointBlocked(map, socketWorld.x + dx * s, socketWorld.y + dy * s)) {
            return true;
        }
    }
    return PointBlocked(map, socketWorld.x + dx * length, socketWorld.y + dy * length);
}

void ChainSpinAction::Update(float dt, MapChip2D* map, Player2D* player, Chain2D* chain, const Vector3& socketWorld) {
    // 縁は1フレームだけ有効
    bool press = pressEdge_;
    bool release = releaseEdge_;
    pressEdge_ = false;
    releaseEdge_ = false;

    if (!player || !chain) {
        return;
    }
    if (player->IsDead() || player->IsGoal()) {
        if (state_ != State::kIdle) {
            Cancel(player, chain);
        }
        return;
    }

    switch (state_) {
    case State::kIdle:
        // 地上ならどこでも宝石を頭上に掲げられる（投げて振り子に入れるのは回せる場所だけ）
        if (press && player->IsOnGround()) {
            StartHold(player, chain, socketWorld);
        }
        break;

    case State::kHold: {
        // 持っている間：足場を離れたら落とす。W を離したら投げずに落とす
        if (!player->IsOnGround() || release || !keyHeld_) {
            Cancel(player, chain);
            break;
        }
        player->SetActionInputModifier(params_.spinMoveFactor_, true);
        // 宝石は頭上（手の真上 holdOffset_）に掲げ、鎖はその間に畳まれている
        radius_ = params_.holdOffset_;
        theta_ = kPi;
        UpdateSpinTarget(socketWorld);

        // A/D で押した方向へ投げる → 振り子開始（回せる場所＝木の板の上でだけ。それ以外では掲げたまま）
        if (IsSpinAllowed()) {
            if (swingInput_ > 0.5f) {
                StartThrow(1.0f, socketWorld);
            } else if (swingInput_ < -0.5f) {
                StartThrow(-1.0f, socketWorld);
            }
        }
        break;
    }

    case State::kStance: {
        // 地上専用：構え中に足場を離れたら解除する
        if (!player->IsOnGround()) {
            Break(dt, player, chain, socketWorld);
            break;
        }
        // 離した瞬間：鎖は前フレームの拘束位置（= 現在の theta_）にあるので、角度を進める前に放つ
        if (release || !keyHeld_) {
            Launch(dt, player, chain, socketWorld);
            break;
        }

        // 構え中は移動不可（A/Dは振りに使う）。ジャンプも無効
        player->SetActionInputModifier(params_.spinMoveFactor_, true);

        // 投げた直後は棒が手元から鎖の実長まで伸びていく
        float full = FullRadius(chain);
        throwOutTime_ += dt;
        float ramp = std::clamp(throwOutTime_ / (std::max)(0.001f, params_.throwOutTime_), 0.0f, 1.0f);
        radius_ = params_.holdOffset_ + (full - params_.holdOffset_) * EaseOut(ramp);
        radius_ = (std::max)(radius_, kMinSpinRadius);

        // 振り子のシミュレーション（真下=0。重力の復元 + 自分で振る力 ÷ 重さ）
        // 重い（宝石＋鎖）ほど振りにくい。振れている向きに合わせて交互に押すと振幅が増し、やがて一回転する
        // 振る力は半径で割らない（割ると長い鎖ほど弱くなり「長いほど遠くへ」と逆になる）
        effMass_ = EffectiveMass(player);
        float g = std::fabs(params_.gravity_);
        float alphaGravity = -(g / radius_) * std::sin(theta_);
        float alphaSwing = params_.swingStrength_ * swingInput_ / effMass_;
        omega_ += (alphaGravity + alphaSwing) * dt;
        omega_ *= (std::max)(0.0f, 1.0f - params_.swingDamping_ * dt);
        theta_ = WrapAngle(theta_ + omega_ * dt);

        // 棒がブロックや地面に入る角度に来たら、縮めずに構えを解除する（鎖は勢いのまま物理に戻り、宝石は地形に当たって落ちる）
        if (IsRodBlocked(map, socketWorld, theta_, radius_, chain->GetEndWeight().radius)) {
            Break(dt, player, chain, socketWorld);
            break;
        }
        UpdateSpinTarget(socketWorld);
        break;
    }

    case State::kCooldown:
        // 連打で浮遊し続けるのを防ぐ。着地でも解除
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
    theta_ = kPi; // 真上
    throwOutTime_ = 0.0f;
    lastBrokeByTerrain_ = false;

    // 飛ぶ速さの上限は通常ジャンプ初速基準（通常ジャンプより高く飛べないので重さのデメリットが残る）
    launchCap_ = player->GetParams().jumpPower_ * params_.launchMaxJumpRatio_;
    effMass_ = EffectiveMass(player);

    // 宝石を頭上に掲げる：鎖全体をソケット→真上 holdOffset_ の直線上に畳む（拘束中は物理を止める）
    radius_ = params_.holdOffset_;
    UpdateSpinTarget(socketWorld);
    chain->SetRigidLineTarget(&spinTarget_);
    player->SetActionInputModifier(params_.spinMoveFactor_, true);
}

void ChainSpinAction::StartThrow(float dirSign, const Vector3& socketWorld) {
    // 押した方向へ放り出す：投げ角（真下=0。180度で真上＝頭上から振り下ろす）から、投げの角速度で振り子が始まる
    state_ = State::kStance;
    theta_ = WrapAngle(dirSign * params_.throwAngleDeg_ * kPi / 180.0f);
    // 角速度の符号は「宝石が押した方向（左右）へ動く」ように決める
    // （接線の x 成分は cosθ × sign(ω)。下半分では +ω が右向きだが、上半分（頭上）では -ω が右向きになる）
    float c = std::cos(theta_);
    float motionSign = (std::fabs(c) > 0.01f) ? dirSign * ((c > 0.0f) ? 1.0f : -1.0f) : dirSign;
    omega_ = motionSign * params_.throwOmega_;
    throwOutTime_ = 0.0f;
    radius_ = params_.holdOffset_;
    UpdateSpinTarget(socketWorld);
    Log("ChainSpinAction: throw dir=" + std::to_string(dirSign) + "\n");
}

void ChainSpinAction::Launch(float dt, Player2D* player, Chain2D* chain, const Vector3& socketWorld) {
    // 鎖全体に回転速度（v = ω × r）を与えて物理に戻す。重りは接線方向へ、途中の鎖も一体で飛ぶ
    float throwSpeed = GetCurrentThrowSpeed();
    chain->ReleaseRigidLine(socketWorld, omega_, params_.weightThrowScale_, dt);
    // 注意: ここで ResetDynamics() を呼ぶと注入した速度が消える

    // プレイヤーも同じ方向（宝石の進行方向）へ、その場で飛ばす。速さは上限（通常ジャンプ初速 × 倍率）で頭打ち
    float speed = std::clamp(throwSpeed * params_.pullTransfer_, 0.0f, launchCap_);
    Vector3 dir = TangentDirection();
    // 真横・下寄りでも床に貼り付かないよう、最低限の上向き成分を確保してから正規化する
    dir.y = (std::max)(dir.y, params_.launchMinUpward_);
    float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
    if (len > 1e-6f) {
        dir.x /= len;
        dir.y /= len;
    } else {
        dir = { 0.0f, 1.0f, 0.0f };
    }
    dir.z = 0.0f;
    // 横方向は PlayerState::launchVelocityX_ として着地・壁接触まで残り、通常の移動入力が重なる
    player->Launch({ dir.x * speed, dir.y * speed, 0.0f });
    player->SetActionInputModifier(1.0f, false);

    lastLaunchSpeed_ = speed;
    lastLaunchDir_ = dir;
    lastBrokeByTerrain_ = false;
    state_ = State::kCooldown;
    cooldownTimer_ = params_.spinCooldown_;
    Log("ChainSpinAction: Launch speed=" + std::to_string(speed) + " / cap " + std::to_string(launchCap_) +
        " (throw " + std::to_string(throwSpeed) + ")" +
        " dir=(" + std::to_string(dir.x) + ", " + std::to_string(dir.y) + ")\n");
}

void ChainSpinAction::Break(float dt, Player2D* player, Chain2D* chain, const Vector3& socketWorld) {
    // 地形に当たった／足場を離れた：鎖は勢いのまま物理に戻すが、プレイヤーは飛ばさない
    chain->ReleaseRigidLine(socketWorld, omega_, params_.weightThrowScale_, dt);
    player->SetActionInputModifier(1.0f, false);
    lastBrokeByTerrain_ = true;
    state_ = State::kCooldown;
    cooldownTimer_ = params_.spinCooldown_;
    Log("ChainSpinAction: stance broken (terrain/airborne) omega=" + std::to_string(omega_) + "\n");
}

void ChainSpinAction::Cancel(Player2D* player, Chain2D* chain) {
    if ((state_ == State::kHold || state_ == State::kStance) && chain) {
        chain->SetRigidLineTarget(nullptr); // 鎖を物理に戻す（速度は注入しない。持っていた宝石はその場から落ちる）
    }
    if (player) {
        player->SetActionInputModifier(1.0f, false);
    }
    state_ = State::kIdle;
    omega_ = 0.0f;
    throwOutTime_ = 0.0f;
    cooldownTimer_ = 0.0f;
}

void ChainSpinAction::DrawImGui() {
#ifdef USE_IMGUI
    const char* stateNames[] = { "Idle", "Hold", "Stance", "Cooldown" };
    ImGui::Text("Spin: %s  theta %.2f  omega %.2f  radius %.2f  mass %.2f  swing %+.0f%s",
                stateNames[static_cast<int>(state_)], theta_, omega_, radius_, effMass_, swingInput_,
                lastBrokeByTerrain_ ? "  [last: broken by terrain]" : "");
    ImGui::Text("Throw now: %.1f u/s -> fly %.1f / cap %.1f %s  (last %.1f, dir %.2f, %.2f)",
                GetCurrentThrowSpeed(), GetCurrentThrowSpeed() * params_.pullTransfer_, launchCap_,
                IsLaunchReady() ? "[READY]" : "", lastLaunchSpeed_, lastLaunchDir_.x, lastLaunchDir_.y);
#endif
}
