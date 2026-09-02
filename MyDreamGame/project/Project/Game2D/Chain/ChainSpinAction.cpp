#include "ChainSpinAction.h"
#include "Game2D/Chain/Chain2D.h"
#include "Game2D/Player/Player2D.h"
#include "Game2D/MapChip2D.h"
#include "Core/Utility/UtilityFunctions.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace {
    constexpr float kPi = std::numbers::pi_v<float>;
    // 壁で半径を縮める時の下限（これ以下では回さない）
    constexpr float kMinSpinRadius = 0.3f;
    // 壁で半径を縮める試行回数と縮小率
    constexpr int kWallShrinkSteps = 6;
    constexpr float kWallShrinkRate = 0.8f;
    // 棒（手→重り）がブロックに入っていないかを調べる間隔と開始距離
    // （手元付近は壁張り付き時にブロック内へ入り得るので少し離れた所から調べる）
    constexpr float kRodProbeStep = 0.25f;
    constexpr float kRodProbeStart = 0.5f;
    // 壁で縮めた半径を戻す速度（チップ/秒）。急に戻すと拘束された鎖が跳ぶ
    constexpr float kRadiusGrowRate = 4.0f;
    // 発射準備完了とみなす上限に対する割合（お宝を明るくする合図）
    constexpr float kLaunchReadyRatio = 0.8f;
    // これより遅い重りには引かれない（壁や床で止まった）
    constexpr float kMinPullSpeed = 1.0f;

    // 振り子の角度（真下=0）を +x軸基準の角度に変換する
    float ThetaToAngleFromX(float theta) { return theta - kPi * 0.5f; }
    // 振り子の角度を [-π, π] に保つ（一回転しても sin/cos の精度を落とさない）
    float WrapAngle(float a) {
        while (a > kPi) a -= 2.0f * kPi;
        while (a < -kPi) a += 2.0f * kPi;
        return a;
    }
    // 手→重りの棒が長さ length の範囲でブロックに入るか
    bool RodBlocked(MapChip2D* map, const Vector3& center, float cs, float sn, float length) {
        auto blockedAt = [&](float s) {
            float x = center.x + cs * s;
            float y = center.y + sn * s;
            return map->GetChipType(map->WorldToChipX(x), map->WorldToChipY(y)) == MapChip2D::ChipType::kBlock;
        };
        for (float s = kRodProbeStart; s < length; s += kRodProbeStep) {
            if (blockedAt(s)) return true;
        }
        return blockedAt(length);
    }
}

void ChainSpinAction::Initialize(const ChainParams& params) {
    params_ = params;
    state_ = State::kIdle;
    keyHeld_ = wasHeld_ = pressEdge_ = releaseEdge_ = false;
    swingInput_ = 0.0f;
    theta_ = 0.0f;
    omega_ = 0.0f;
    radius_ = 0.0f;
    effMass_ = 1.0f;
    launchCap_ = 0.0f;
    cooldownTimer_ = 0.0f;
    throwSpeed_ = 0.0f;
    thrownTimer_ = 0.0f;
    lastLaunchSpeed_ = 0.0f;
    lastLaunchDir_ = { 0.0f, 0.0f, 0.0f };
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
    // 振り子の状態も入力から決まるので一緒に戻す（前回のプレイ/再生ループの値が残ると0フレーム目からずれる）
    theta_ = 0.0f;
    omega_ = 0.0f;
    radius_ = 0.0f;
    throwSpeed_ = 0.0f;
    thrownTimer_ = 0.0f;
}

float ChainSpinAction::GetCurrentThrowSpeed() const {
    // 遠心力：接線速度 = |ω| × r（自分で振って得た勢いそのもの）
    return std::fabs(omega_) * radius_ * params_.weightThrowScale_;
}

bool ChainSpinAction::IsLaunchReady() const {
    if (state_ != State::kStance || launchCap_ <= 0.0f) {
        return false;
    }
    float pull = GetCurrentThrowSpeed() * params_.pullTransfer_;
    return pull >= launchCap_ * kLaunchReadyRatio;
}

float ChainSpinAction::EffectiveMass(Player2D* player) const {
    // 宝石の質量 + 鎖の質量（ユニット数 × 1ユニットの質量）。振る力をこれで割る
    float units = player ? static_cast<float>((std::max)(0, player->GetChainLength())) : 0.0f;
    return (std::max)(0.1f, params_.treasureMass_ + units * params_.chainMassPerUnit_);
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
        // 地上専用（空中では構えられない）
        if (press && player->IsOnGround()) {
            StartStance(player, chain, socketWorld, map);
        }
        break;

    case State::kStance: {
        // 地上専用：構え中に足場を離れたら中断する（空中発射の抜け道を防ぐ）
        if (!player->IsOnGround()) {
            Cancel(player, chain);
            break;
        }
        // 離した瞬間：鎖は前フレームの拘束位置（= 現在の theta_）にあるので、角度を進める前に放つ
        if (release || !keyHeld_) {
            Throw(dt, player, chain, socketWorld);
            break;
        }

        // 構え中は移動不可（A/Dは振りに使う）。ジャンプも無効
        player->SetActionInputModifier(params_.spinMoveFactor_, true);

        // 振り子のシミュレーション（真下=0。重力の復元 + 自分で振る力 ÷ 重さ）
        // 重い（宝石＋鎖）ほど振りにくい。振れている向きに合わせて交互に押すと振幅が増し、やがて一回転する
        // 振る力は半径で割らない（割ると長い鎖ほど弱くなり「長いほど遠くへ」と逆になる。
        // 数値検証: S=40 で 1ユニットは上限に届かず、3ユニット≒3.2秒、6ユニット≒2.0秒で上限到達）
        effMass_ = EffectiveMass(player);
        float g = std::fabs(params_.gravity_);
        float alphaGravity = -(g / radius_) * std::sin(theta_);
        float alphaSwing = params_.swingStrength_ * swingInput_ / effMass_;
        omega_ += (alphaGravity + alphaSwing) * dt;
        omega_ *= (std::max)(0.0f, 1.0f - params_.swingDamping_ * dt);
        theta_ = WrapAngle(theta_ + omega_ * dt);

        // 壁で縮めた半径は急に戻さない（拘束された鎖が跳ぶのを防ぐ。縮める方向は即時）
        float angleFromX = ThetaToAngleFromX(theta_);
        float targetRadius = ComputeRadius(map, chain, socketWorld, angleFromX, chain->GetEndWeight().radius);
        if (targetRadius < radius_) {
            radius_ = targetRadius;
        } else {
            radius_ = (std::min)(targetRadius, radius_ + kRadiusGrowRate * dt);
        }
        // 真下=0 の角度なので位置は (sinθ, -cosθ)。鎖全体はソケット→ここの直線上に拘束される
        spinTarget_ = { socketWorld.x + std::sin(theta_) * radius_,
                        socketWorld.y - std::cos(theta_) * radius_,
                        0.0f };
        break;
    }

    case State::kThrown: {
        // 重りが先に飛び、少し遅れてその進行方向へプレイヤーが引かれる。プレイヤーの操作はもう自由
        thrownTimer_ += dt;
        if (thrownTimer_ < params_.pullDelay_) {
            break;
        }
        Vector3 endVel = chain->GetEndVelocity();
        float speed = std::sqrt(endVel.x * endVel.x + endVel.y * endVel.y);
        if (speed >= kMinPullSpeed) {
            Vector3 dir = { endVel.x / speed, endVel.y / speed, 0.0f };
            Yank(player, dir, speed);
            state_ = State::kCooldown;
            cooldownTimer_ = params_.spinCooldown_;
        } else {
            // 重りが壁や床で止まっていた。引っ張らずに終了
            state_ = State::kIdle;
        }
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

void ChainSpinAction::StartStance(Player2D* player, Chain2D* chain, const Vector3& socketWorld, MapChip2D* map) {
    state_ = State::kStance;
    omega_ = 0.0f;

    // 重りが今ある方向から始める（いきなり跳ばない）。真下=0 の角度に変換
    Vector3 end = chain->GetEndPosition();
    float dx = end.x - socketWorld.x;
    float dy = end.y - socketWorld.y;
    float angleFromX = (dx * dx + dy * dy > 1e-6f) ? std::atan2(dy, dx) : -kPi * 0.5f;
    theta_ = WrapAngle(angleFromX + kPi * 0.5f);

    // 引っ張られる速さの上限は通常ジャンプ初速基準（通常ジャンプより高く飛べないので重さのデメリットが残る）
    launchCap_ = player->GetParams().jumpPower_ * params_.launchMaxJumpRatio_;
    effMass_ = EffectiveMass(player);

    radius_ = ComputeRadius(map, chain, socketWorld, angleFromX, chain->GetEndWeight().radius);
    spinTarget_ = { socketWorld.x + std::sin(theta_) * radius_,
                    socketWorld.y - std::cos(theta_) * radius_,
                    0.0f };

    // 鎖全体をソケット→重りの直線上に拘束（ピンと張った棒として回す。途中の鎖はたるまない）
    chain->SetRigidLineTarget(&spinTarget_);
    player->SetActionInputModifier(params_.spinMoveFactor_, true);
}

void ChainSpinAction::Throw(float dt, Player2D* player, Chain2D* chain, const Vector3& socketWorld) {
    // 鎖全体に回転速度（v = ω × r）を与えて物理に戻す。重りは接線方向へ、途中の鎖も一体で飛ぶ
    throwSpeed_ = GetCurrentThrowSpeed();
    chain->ReleaseRigidLine(socketWorld, omega_, params_.weightThrowScale_, dt);
    // 注意: ここで ResetDynamics() を呼ぶと注入した速度が消える

    // 構えを解いてプレイヤーの操作を自由にする
    player->SetActionInputModifier(1.0f, false);

    state_ = State::kThrown;
    thrownTimer_ = 0.0f;
    Log("ChainSpinAction: Throw speed=" + std::to_string(throwSpeed_) +
        " radius=" + std::to_string(radius_) + " omega=" + std::to_string(omega_) + "\n");
}

void ChainSpinAction::Yank(Player2D* player, const Vector3& moveDir, float weightSpeed) {
    // 引く速さ：その時点の重りの速さ × 伝達率。上限は通常ジャンプ初速 × 倍率
    float speed = std::clamp(weightSpeed * params_.pullTransfer_, 0.0f, launchCap_);

    // 真横・下寄りでも床に貼り付かないよう、最低限の上向き成分を確保してから正規化する
    Vector3 dir = moveDir;
    dir.y = (std::max)(dir.y, params_.launchMinUpward_);
    float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
    if (len > 1e-6f) {
        dir.x /= len;
        dir.y /= len;
    } else {
        dir = { 0.0f, 1.0f, 0.0f };
    }
    dir.z = 0.0f;

    // プレイヤーを重りの進行方向へ（横方向は PlayerState::launchVelocityX_ として着地・壁接触まで残り、通常の移動入力が重なる）
    player->Launch({ dir.x * speed, dir.y * speed, 0.0f });

    lastLaunchSpeed_ = speed;
    lastLaunchDir_ = dir;
    Log("ChainSpinAction: Yank speed=" + std::to_string(speed) + " / cap " + std::to_string(launchCap_) +
        " (weight " + std::to_string(weightSpeed) + ")" +
        " dir=(" + std::to_string(dir.x) + ", " + std::to_string(dir.y) + ")\n");
}

void ChainSpinAction::Cancel(Player2D* player, Chain2D* chain) {
    if (state_ == State::kStance && chain) {
        chain->SetRigidLineTarget(nullptr); // 鎖を物理に戻す（速度は注入しない）
    }
    if (player) {
        player->SetActionInputModifier(1.0f, false);
    }
    state_ = State::kIdle;
    omega_ = 0.0f;
    cooldownTimer_ = 0.0f;
    thrownTimer_ = 0.0f;
}

float ChainSpinAction::ComputeRadius(MapChip2D* map, Chain2D* chain, const Vector3& center, float angleFromX, float endRadius) const {
    // 鎖の実長 × spinRadiusRatio_（1.0 で節間隔ちょうどのピンと張った棒）
    // 長い鎖ほど半径が大きい = 同じ振り角でも接線速度が大きい
    float radius = (std::min)(params_.spinRadiusMax_, chain->GetTotalLength() * params_.spinRadiusRatio_);
    radius = (std::max)(radius, kMinSpinRadius);

    // 棒のどこかがブロックに入るならその角度だけ半径を縮める（壁貫通の見た目を抑える）
    if (map) {
        float cs = std::cos(angleFromX);
        float sn = std::sin(angleFromX);
        for (int step = 0; step < kWallShrinkSteps; ++step) {
            if (!RodBlocked(map, center, cs, sn, radius + endRadius)) {
                break;
            }
            radius *= kWallShrinkRate;
            if (radius < kMinSpinRadius) {
                radius = kMinSpinRadius;
                break;
            }
        }
    }
    return radius;
}

void ChainSpinAction::DrawImGui() {
#ifdef USE_IMGUI
    const char* stateNames[] = { "Idle", "Stance", "Thrown", "Cooldown" };
    ImGui::Text("Spin: %s  theta %.2f  omega %.2f  radius %.2f  mass %.2f  swing %+.0f",
                stateNames[static_cast<int>(state_)], theta_, omega_, radius_, effMass_, swingInput_);
    ImGui::Text("Throw now: %.1f u/s -> pull %.1f / cap %.1f %s  (last pull %.1f, dir %.2f, %.2f)",
                GetCurrentThrowSpeed(), GetCurrentThrowSpeed() * params_.pullTransfer_, launchCap_,
                IsLaunchReady() ? "[READY]" : "", lastLaunchSpeed_, lastLaunchDir_.x, lastLaunchDir_.y);
#endif
}
