#pragma once
#include "Game2D/Chain/ChainConfig.h"
#include "Core/Utility/Vector3.h"

class Player2D;
class Chain2D;
class MapChip2D;

/// <summary>
/// スピンジャンプ：木の板の上で構えて（W押し続け）ピンと張った鎖を自分で左右に振り、離すとその勢いの方向へ飛ぶ
/// - 構え中は鎖全体を「手を支点にした剛体の棒」として拘束する（Chain2D::SetRigidLineTarget。途中の鎖は物理を止める）
///   角加速度 = 重力の復元 + A/D の振る力 ÷（宝石の質量 + 鎖の質量）。重いほど振りにくく、振りの向きに合わせて交互に押すと振幅が増す
///   （振る力は半径で割らない。長い鎖は同じ力でも接線速度が伸びるので、短い鎖は上限に届かず長い鎖ほど早く上限に届く）
/// - 半径は鎖の実長で固定。棒（手→宝石）がブロックや地面に当たる角度に入ったら構えは解除される（縮めない）。
///   鎖は回転の勢いを持ったまま物理に戻り、宝石は地形に当たって落ちる。プレイヤーは飛ばない
/// - W を離すと鎖全体に回転速度を与えて物理に戻し、同時にプレイヤーを宝石の進行方向（接線）へ飛ばす
///   速さ = |ω| × r × weightThrowScale_ × pullTransfer_。上限は通常ジャンプ初速 × launchMaxJumpRatio_（通常ジャンプより高くは飛べない）
///   横の勢いは PlayerState::launchVelocityX_ として着地・壁接触まで残り、通常の移動入力が重なる
/// - 構え中はプレイヤーは移動できない（A/Dは振りに使う）
/// 状態遷移: Idle ─(W押下&地上&板の上)→ Stance ─(W離す)→ Launch → Cooldown ─(時間経過/着地)→ Idle
///           Stance ─(棒が地形に当たる/足場を離れる)→ Break → Cooldown / Stance ─(死亡/ゴール/外す/拾う/巻き戻し/リセット)→ Cancel → Idle
/// 固定 dt と入力だけで状態が決まるのでリプレイで再現する
/// </summary>
class ChainSpinAction {
public:
    enum class State {
        kIdle,
        kStance,   // 構え中（張った鎖を漕いでいる）
        kCooldown,
    };

    void Initialize(const ChainParams& params);
    void SetParams(const ChainParams& params) { params_ = params; }

    /// <summary>毎フレーム HandleInput から呼ぶ。W の押した/離したの縁は内部で検出する</summary>
    void SetKeyHeld(bool held);

    /// <summary>構え中の振り入力（-1:左 / 0 / +1:右）。毎フレーム HandleInput から呼ぶ</summary>
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
    bool IsInStance() const { return state_ == State::kStance; }
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
    void StartStance(Player2D* player, Chain2D* chain, const Vector3& socketWorld);
    // W を離した：鎖ごと放ち、プレイヤーを宝石の進行方向へ飛ばす
    void Launch(float dt, Player2D* player, Chain2D* chain, const Vector3& socketWorld);
    // 棒が地形に当たった：鎖を勢い付きで物理に戻して構えを解除する（プレイヤーは飛ばない）
    void Break(float dt, Player2D* player, Chain2D* chain, const Vector3& socketWorld);
    // 鎖の実長から半径を決める
    float ComputeRadius(Chain2D* chain) const;
    // 手→宝石の棒（角度 theta、長さ radius + 宝石の半径）が地形に入るか
    bool IsRodBlocked(MapChip2D* map, const Vector3& socketWorld, float theta, float radius, float endRadius) const;
    // 振りにくさ：宝石の質量 + 鎖の質量（ユニット数 × chainMassPerUnit_）
    float EffectiveMass(Player2D* player) const;
    // 宝石の進行方向（接線）
    Vector3 TangentDirection() const;

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
    float radius_ = 0.0f;       // 回転半径（鎖の実長で固定）
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
