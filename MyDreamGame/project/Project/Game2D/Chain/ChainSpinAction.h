#pragma once
#include "Game2D/Chain/ChainConfig.h"
#include "Core/Utility/Vector3.h"

class Player2D;
class Chain2D;
class MapChip2D;

/// <summary>
/// スピンジャンプ：木の板の上で W を押して宝石を手に持ち、A/D でその方向へ投げると振り子になる。振って勢いをつけ、W を離すとその方向へ飛ぶ
/// - Hold（W押下）：宝石を手元に引き寄せて持つ（鎖は手の中に畳まれる）。移動不可。W を離せば投げずに落とすだけ
/// - Throw（Hold 中に A/D）：押した方向へ宝石を放り出す。棒（手→宝石）が throwOutTime_ で鎖の実長まで伸び、
///   throwAngleDeg_ の角度・throwOmega_ の角速度から振り子が始まる（ここから Stance）
/// - Stance：鎖全体を「手を支点にした剛体の棒」として拘束する（Chain2D::SetRigidLineTarget。途中の鎖は物理を止める）
///   角加速度 = 重力の復元 + A/D の振る力 ÷（宝石の質量 + 鎖の質量）。重いほど振りにくく、振りの向きに合わせて交互に押すと振幅が増す
///   棒がブロックや地面に当たる角度に入ったら構えは解除される（縮めない）。鎖は勢いのまま物理に戻り、宝石は地形に当たって落ちる
/// - Launch（Stance 中に W を離す）：鎖全体に回転速度を与えて物理に戻し、同時にプレイヤーを宝石の進行方向（接線）へ飛ばす
///   速さ = |ω| × r × weightThrowScale_ × pullTransfer_。上限は通常ジャンプ初速 × launchMaxJumpRatio_（通常ジャンプより高くは飛べない）
/// 状態遷移: Idle ─(W押下&地上&板の上)→ Hold ─(A/D)→ Stance ─(W離す)→ Launch → Cooldown ─(時間経過/着地)→ Idle
///           Hold ─(W離す/足場を離れる)→ Cancel → Idle / Stance ─(棒が地形に当たる/足場を離れる)→ Break → Cooldown
///           Hold・Stance ─(死亡/ゴール/外す/拾う/巻き戻し/リセット)→ Cancel → Idle
/// 固定 dt と入力だけで状態が決まるのでリプレイで再現する
/// </summary>
class ChainSpinAction {
public:
    enum class State {
        kIdle,
        kHold,     // 宝石を手に持っている（まだ投げていない）
        kStance,   // 投げた後、張った鎖を漕いでいる
        kCooldown,
    };

    void Initialize(const ChainParams& params);
    void SetParams(const ChainParams& params) { params_ = params; }

    /// <summary>毎フレーム HandleInput から呼ぶ。W の押した/離したの縁は内部で検出する</summary>
    void SetKeyHeld(bool held);

    /// <summary>振り入力（-1:左 / 0 / +1:右）。Hold 中は投げる方向、Stance 中は漕ぐ方向。毎フレーム HandleInput から呼ぶ</summary>
    void SetSwingInput(float inputX) { swingInput_ = inputX; }

    /// <summary>今フレーム、回せる場所（木の板の上）にいるか。毎フレーム ChainManager が設定する</summary>
    void SetSpinAllowed(bool allowed) { spinAllowed_ = allowed; }
    /// <summary>構えを始められる場所か（木の板の上、または spinAnywhere_）</summary>
    bool IsSpinAllowed() const { return spinAllowed_ || params_.spinAnywhere_; }

    /// <summary>プレイヤー鎖の物理更新の前に呼ぶ（拘束先 spinTarget_ を先に決める）</summary>
    void Update(float dt, MapChip2D* map, Player2D* player, Chain2D* chain, const Vector3& socketWorld);

    /// <summary>中断。鎖を物理に戻し、プレイヤーの入力修飾も解除する（死亡・外す・拾う・巻き戻し・リセット時）</summary>
    void Cancel(Player2D* player, Chain2D* chain);

    /// <summary>
    /// 入力の縁検出と振り子状態をクリアする（プレイ開始・リプレイ再生開始・巻き戻し時）
    /// これを怠ると「Playを押す前からWを押していたか」で0フレーム目の挙動が変わり、リプレイがずれる
    /// </summary>
    void ResetInputState();

    State GetState() const { return state_; }
    /// <summary>持っている〜漕いでいる間（鎖が剛体拘束されている間）</summary>
    bool IsInStance() const { return state_ == State::kHold || state_ == State::kStance; }
    bool IsHolding() const { return state_ == State::kHold; }
    /// <summary>振り子の角度（真下=0、rad）</summary>
    float GetTheta() const { return theta_; }
    float GetOmega() const { return omega_; }
    float GetRadius() const { return radius_; }
    /// <summary>今離した場合の重りの速さ（|ω|×r×倍率）</summary>
    float GetCurrentThrowSpeed() const;
    /// <summary>飛ぶ速さの上限（構え開始時の通常ジャンプ初速 × launchMaxJumpRatio_）</summary>
    float GetLaunchCap() const { return launchCap_; }
    /// <summary>上限の8割以上の勢いがついているか（合図用）</summary>
    bool IsLaunchReady() const;
    float GetLastLaunchSpeed() const { return lastLaunchSpeed_; }
    const Vector3& GetLastLaunchDirection() const { return lastLaunchDir_; }

    void DrawImGui();

private:
    // 宝石を手に引き寄せて持つ
    void StartHold(Player2D* player, Chain2D* chain, const Vector3& socketWorld);
    // 持っている宝石を dirSign（-1:左 / +1:右）へ投げ、振り子を始める
    void StartThrow(float dirSign, const Vector3& socketWorld);
    // W を離した：鎖ごと放ち、プレイヤーを宝石の進行方向へ飛ばす
    void Launch(float dt, Player2D* player, Chain2D* chain, const Vector3& socketWorld);
    // 棒が地形に当たった／足場を離れた：鎖を勢い付きで物理に戻して構えを解除する（プレイヤーは飛ばない）
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

    ChainParams params_;
    State state_ = State::kIdle;

    // 入力（縁検出）
    bool keyHeld_ = false;
    bool wasHeld_ = false;
    bool pressEdge_ = false;
    bool releaseEdge_ = false;
    float swingInput_ = 0.0f;
    bool spinAllowed_ = false; // 回せる場所の上にいる（毎フレーム更新）

    // 振り子状態
    float theta_ = 0.0f;        // 真下を0とした角度（rad。+で右へ振れる）
    float omega_ = 0.0f;        // 角速度（rad/s）
    float radius_ = 0.0f;       // 現在の回転半径（投げた直後は手元から鎖の実長まで伸びる）
    float throwOutTime_ = 0.0f; // 投げてからの経過秒（半径の伸びに使う）
    float holdTime_ = 0.0f;     // 掲げてからの経過秒（直後の A/D は投げ入力にしない）
    float effMass_ = 1.0f;      // 現在の振りにくさ（ImGui表示用）
    float launchCap_ = 0.0f;    // 飛ぶ速さの上限（構え開始時に決定）
    float cooldownTimer_ = 0.0f;

    // 直近の発射（ImGui確認用）
    float lastLaunchSpeed_ = 0.0f;
    Vector3 lastLaunchDir_ = { 0.0f, 0.0f, 0.0f };
    bool lastBrokeByTerrain_ = false;

    // 重り（末端ノード）の拘束先。メンバなのでポインタが安定する
    Vector3 spinTarget_ = { 0.0f, 0.0f, 0.0f };
};
