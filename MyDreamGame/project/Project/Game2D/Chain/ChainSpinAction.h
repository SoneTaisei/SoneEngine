#pragma once
#include "Game2D/Chain/ChainConfig.h"
#include "Core/Utility/Vector3.h"
#include <memory>
#include <vector>

class Player2D;
class Chain2D;
class MapChip2D;
class PrimitiveObject;

/// <summary>
/// スピンジャンプ（振り子）：Q で宝石を手に持ち、A/D でその方向へ投げると振り子になる。振って勢いをつけ、SPACE で宝石の進行方向へ飛ぶ
/// - Hold（Q）：宝石を手元に引き寄せて持つ。移動不可。もう一度 Q で落とす。SPACE なら落として通常ジャンプ
/// - Throw（Hold 中に A/D）：押した方向へ宝石を放り出す。棒（手→宝石）が throwOutTime_ で鎖の実長まで伸び、
///   throwAngleDeg_ の角度・throwOmega_ の角速度から振り子が始まる（ここから Stance）
/// - Stance：鎖全体を「手を支点にした剛体の棒」として拘束する（Chain2D::SetRigidLineTarget）
///   角加速度 = 重力の復元 + A/D の振る力 ÷（宝石の質量 + 鎖の質量）。振りの向きに合わせて交互に押すと振幅が増す
///   棒がブロックや地面に当たる角度に入ったら構えは解除される。Q でも解除（鎖は勢いのまま物理へ）
/// - Aim（Stance 中に SPACE を押している間）：振り子だけがスロー（aimSlow_ 倍）になり、宝石がどっちへ向いているか見える。漕げない
///   最長 aimMaxTime_ 秒で自動的に飛ぶ。短く押せばほぼ即発射
/// - Launch（SPACE を離した瞬間）：鎖全体に振りの回転速度（スロー前の勢い）を与えて物理に戻し、
///   プレイヤーを宝石の進行方向（接線）へ飛ばす。速さ = |ω| × r × weightThrowScale_ × pullTransfer_、上限は通常ジャンプ初速 × launchMaxJumpRatio_
///   アシスト：接線が launchAngleDeg_ ± justWindowDeg_ の時（ジャスト）は方向をそろえて justBonus_ 倍、
///   それ以外は向きを上向きのコーン（coneMinDeg_〜coneMaxDeg_）に収める
/// 表示（Stance 中）：宝石の後ろの残像 = 進んでいる向き。宝石はジャストに近づくと光る
///   SPACE を押している間（スロー中）だけ、プレイヤーの体から「今離したら自分が飛ぶ向き」へ矢じりが出る（アシスト込み。勢いが強いほど長い）
///   矢じりは 暗い = 勢い不足、金 = 飛べる、白っぽい金 = ジャスト
/// 状態遷移: Idle ─(Q&地上)→ Hold ─(A/D&板の上)→ Stance ─(SPACE 離す)→ Launch → Cooldown ─(時間経過/着地)→ Idle
///           Hold ─(Q/SPACE/足場を離れる)→ Cancel → Idle / Stance ─(Q/棒が地形に当たる/足場を離れる)→ Break → Cooldown
///           Hold・Stance ─(死亡/ゴール/外す/拾う/巻き戻し/リセット)→ Cancel → Idle
/// 固定 dt と入力だけで状態が決まるのでリプレイで再現する（ただし Q はリプレイの記録キーに無いので、再生では持てない）
/// </summary>
class ChainSpinAction {
public:
    enum class State {
        kIdle,
        kHold,     // 宝石を手に持っている（まだ投げていない）
        kStance,   // 投げた後、張った鎖を漕いでいる（SPACE 押下中はスローで狙っている）
        kCooldown,
    };

    void Initialize(const ChainParams& params);
    void SetParams(const ChainParams& params) { params_ = params; }

    /// <summary>Q が押された瞬間。Idle なら持つ、Hold / Stance ならやめる</summary>
    void OnHoldToggle() { toggleEdge_ = true; }
    /// <summary>SPACE を押しているか（Stance 中だけ ChainManager が毎フレーム渡す）。押した瞬間からスロー、離した瞬間に飛ぶ</summary>
    void SetLaunchHeld(bool held) { launchHeld_ = held; }

    /// <summary>振り入力（-1:左 / 0 / +1:右）。Hold 中は投げる方向、Stance 中は漕ぐ方向。毎フレーム HandleInput から呼ぶ</summary>
    void SetSwingInput(float inputX) { swingInput_ = inputX; }

    /// <summary>今フレーム、回せる場所（木の板の上）にいるか。毎フレーム ChainManager が設定する</summary>
    void SetSpinAllowed(bool allowed) { spinAllowed_ = allowed; }
    /// <summary>構えを始められる場所か（木の板の上、または spinAnywhere_）</summary>
    bool IsSpinAllowed() const { return spinAllowed_ || params_.spinAnywhere_; }

    /// <summary>プレイヤー鎖の物理更新の前に呼ぶ（拘束先 spinTarget_ を先に決める）</summary>
    void Update(float dt, MapChip2D* map, Player2D* player, Chain2D* chain, const Vector3& socketWorld);

    /// <summary>矢じりと残像（Stance 中だけ）。ChainManager::Draw から呼ぶ</summary>
    void Draw();

    /// <summary>中断。鎖を物理に戻し、プレイヤーの入力修飾も解除する（死亡・外す・拾う・巻き戻し・リセット時）</summary>
    void Cancel(Player2D* player, Chain2D* chain);

    /// <summary>
    /// 入力の縁検出と振り子状態をクリアする（プレイ開始・リプレイ再生開始・巻き戻し時）
    /// これを怠ると 0 フレーム目の挙動が変わり、リプレイがずれる
    /// </summary>
    void ResetInputState();

    State GetState() const { return state_; }
    /// <summary>持っている〜漕いでいる間（鎖が剛体拘束されている間）</summary>
    /// <summary>板以外で投げた直後で、まだ動けない間か</summary>
    bool IsThrowLocked() const { return throwLockTime_ >= 0.0f; }
    bool IsInStance() const { return state_ == State::kHold || state_ == State::kStance; }
    bool IsHolding() const { return state_ == State::kHold; }
    /// <summary>SPACE を押してスローで狙っている</summary>
    bool IsAiming() const { return state_ == State::kStance && aiming_; }
    /// <summary>振り子の角度（真下=0、rad）</summary>
    float GetTheta() const { return theta_; }
    float GetOmega() const { return omega_; }
    float GetRadius() const { return radius_; }
    /// <summary>今この瞬間の重りの速さ（|ω|×r×倍率）</summary>
    float GetCurrentThrowSpeed() const;
    /// <summary>飛ぶ速さの上限（構え開始時の通常ジャンプ初速 × launchMaxJumpRatio_）</summary>
    float GetLaunchCap() const { return launchCap_; }
    /// <summary>上限の8割以上の勢いがついているか（合図用）</summary>
    bool IsLaunchReady() const;
    /// <summary>接線が launchAngleDeg_ ± justWindowDeg_ の中（今離せばジャスト）</summary>
    bool IsInJustWindow() const;
    /// <summary>宝石を光らせる強さ（0〜1）。接線がジャスト窓に近づくほど強く、窓の中で 1。勢い不足なら弱い。Stance 以外は 0</summary>
    float GetTimingGlow() const;
    float GetLastLaunchSpeed() const { return lastLaunchSpeed_; }
    const Vector3& GetLastLaunchDirection() const { return lastLaunchDir_; }

    void DrawImGui();

private:
    // 宝石を手に引き寄せて持つ
    void StartHold(Player2D* player, Chain2D* chain, const Vector3& socketWorld);
    // 持っている宝石を dirSign（-1:左 / +1:右）へ投げ、振り子を始める
    void StartThrow(float dirSign, const Vector3& socketWorld);
    /// <summary>板以外の床で A/D：回さずにその場から宝石を投げる（警備員に当てる用）</summary>
    void ThrowFromGround(float dirSign, float dt, Player2D* player, Chain2D* chain);
    // SPACE を離した：鎖ごと放ち、プレイヤーを宝石の進行方向へ飛ばす（just = ジャスト窓の中で離した）
    void Launch(float dt, Player2D* player, Chain2D* chain, const Vector3& socketWorld, bool just);
    // 棒が地形に当たった／足場を離れた／Q でやめた：鎖を勢い付きで物理に戻して構えを解除する（プレイヤーは飛ばない）
    void Break(float dt, Player2D* player, Chain2D* chain, const Vector3& socketWorld);
    // 鎖の実長から伸び切った時の半径を決める
    float FullRadius(Chain2D* chain) const;
    // 手→宝石の棒（角度 theta、長さ radius + 宝石の半径）が地形に入るか
    bool IsRodBlocked(MapChip2D* map, const Vector3& socketWorld, float theta, float radius, float endRadius) const;
    // 振りにくさ：宝石の質量 + 鎖の質量（ユニット数 × chainMassPerUnit_）
    float EffectiveMass(Player2D* player) const;
    // 宝石の進行方向（接線）
    Vector3 TangentDirection() const;
    // 角度と半径から拘束先を決める
    void UpdateSpinTarget(const Vector3& socketWorld);

    // ---- 発射のアシスト ----
    // 接線の向き（度。水平から上向きを正。左右は問わない）
    float TangentAngleDeg() const;
    // 今の勢いで接線が届く最大の上向き角（度）。振幅が小さいと 60 度に届かないので、窓をここに寄せる
    float MaxReachableAngleDeg() const;
    // 窓の中心（min(launchAngleDeg_, 届く最大角)）
    float WindowCenterDeg() const;
    // 向きを上向きのコーンに収める（速さは変えない）
    Vector3 ClampToCone(const Vector3& dir) const;
    // 今離した時の飛ぶ速度（アシスト込み）
    Vector3 PredictLaunchVelocity(bool just) const;

    // ---- 矢じりと残像 ----
    void EnsureVisuals();
    void UpdateVisuals(Player2D* player, const Vector3& socketWorld);

    ChainParams params_;
    State state_ = State::kIdle;

    // 入力
    bool toggleEdge_ = false;   // Q（縁。1フレームだけ有効）
    bool launchHeld_ = false;   // SPACE を押しているか（毎フレーム設定）
    bool launchHeldPrev_ = false;
    float swingInput_ = 0.0f;
    bool spinAllowed_ = false;  // 回せる場所の上にいる（毎フレーム更新）

    // 狙い（SPACE 押下中）
    bool aiming_ = false;
    float aimTimer_ = 0.0f;
    // Q 連打で持つ→やめる→持つ が続かないように、やめた後は少しの間持てない
    float holdBlockTimer_ = 0.0f;
    float throwLockTime_ = -1.0f;    // 板以外で投げてからの経過秒（-1 で投げていない）。この間は動けない
    float throwLockDir_ = 0.0f;      // 投げた向き（押しっぱなしの A/D で歩き出さないよう見張る）

    // 振り子状態
    float theta_ = 0.0f;        // 真下を0とした角度（rad。+で右へ振れる）
    float omega_ = 0.0f;        // 角速度（rad/s）
    float radius_ = 0.0f;       // 現在の回転半径（投げた直後は手元から鎖の実長まで伸びる）
    float throwOutTime_ = 0.0f; // 投げてからの経過秒（半径の伸びに使う）
    float holdTime_ = 0.0f;     // 持ってからの経過秒（直後の A/D は投げ入力にしない）
    float effMass_ = 1.0f;      // 現在の振りにくさ（ImGui表示用）
    float launchCap_ = 0.0f;    // 飛ぶ速さの上限（構え開始時に決定）
    float cooldownTimer_ = 0.0f;

    // 直近の発射（ImGui確認用）
    float lastLaunchSpeed_ = 0.0f;
    Vector3 lastLaunchDir_ = { 0.0f, 0.0f, 0.0f };
    bool lastBrokeByTerrain_ = false;
    bool lastLaunchJust_ = false;

    // 矢じり：スロー中だけ、プレイヤーの体から「今離したら自分が飛ぶ向き」を指す（円錐）
    std::unique_ptr<PrimitiveObject> arrow_;
    bool arrowVisible_ = false;
    // 残像：宝石の後ろ（円周上、進んできた側）に並ぶ薄い球。尾の反対側が進行方向
    std::vector<std::unique_ptr<PrimitiveObject>> trailDots_;
    int trailVisible_ = 0;

    // 重り（末端ノード）の拘束先。メンバなのでポインタが安定する
    Vector3 spinTarget_ = { 0.0f, 0.0f, 0.0f };
};
