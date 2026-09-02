#pragma once
#include "Game2D/Chain/ChainConfig.h"
#include "Core/Utility/Vector3.h"

class Player2D;
class Chain2D;
class MapChip2D;

/// <summary>
/// スピンジャンプ：構えて（W押し続け）ピンと張った鎖を自分で左右に振り、離すと鎖ごと飛んで重りの勢いに引っ張られる
/// - 構え中は鎖全体を「ソケットを支点にした剛体の棒」として拘束する（Chain2D::SetRigidLineTarget。途中の鎖がたるまない）
///   角加速度 = 重力の復元 + A/D の振る力 ÷（宝石の質量 + 鎖の質量）。重いほど振りにくく、振りの向きに合わせて交互に押すと振幅が増す
///   （振る力は半径で割らない。長い鎖は同じ力でも接線速度が伸びるので、短い鎖は上限に届かず長い鎖ほど早く上限に届く）
/// - W を離すと全ノードに回転速度を与えて物理に戻す（鎖ごと一体で飛ぶ）。プレイヤーの操作はこの時点で自由になる
/// - pullDelay_ 秒後、その時点の重りの進行方向へプレイヤーが引っ張られる（重りが先に行き、プレイヤーが後から引かれる）
///   引く速さ = 重りの速さ × pullTransfer_。上限は通常ジャンプ初速 × launchMaxJumpRatio_（通常ジャンプより高くは飛べない）
///   重りが壁や床で勢いを失っていれば弱くなる/引かれない
/// - 構え中はプレイヤーは移動できない（A/Dは振りに使う）。引っ張られた後は通常操作がそのまま重なる
/// 状態遷移: Idle ─(W押下&地上)→ Stance ─(W離す)→ Thrown ─(pullDelay_)→ Yank → Cooldown ─(時間経過/着地)→ Idle
///           Stance・Thrown ─(死亡/ゴール/外す/拾う/巻き戻し/リセット)→ Cancel → Idle
/// 固定 dt と入力だけで状態が決まるのでリプレイで再現する
/// </summary>
class ChainSpinAction {
public:
    enum class State {
        kIdle,
        kStance,   // 構え中（張った鎖を漕いでいる）
        kThrown,   // 放った直後（重りが先に飛んでいる）
        kCooldown,
    };

    void Initialize(const ChainParams& params);
    void SetParams(const ChainParams& params) { params_ = params; }

    /// <summary>毎フレーム HandleInput から呼ぶ。W の押した/離したの縁は内部で検出する</summary>
    void SetKeyHeld(bool held);

    /// <summary>構え中の振り入力（-1:左 / 0 / +1:右）。毎フレーム HandleInput から呼ぶ</summary>
    void SetSwingInput(float inputX) { swingInput_ = inputX; }

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
    bool IsThrown() const { return state_ == State::kThrown; }
    /// <summary>振り子の角度（真下=0、rad）</summary>
    float GetTheta() const { return theta_; }
    float GetOmega() const { return omega_; }
    float GetRadius() const { return radius_; }
    /// <summary>今離した場合の重りの速さ（|ω|×r×倍率）</summary>
    float GetCurrentThrowSpeed() const;
    /// <summary>引っ張られる速さの上限（構え開始時の通常ジャンプ初速 × launchMaxJumpRatio_）</summary>
    float GetLaunchCap() const { return launchCap_; }
    /// <summary>上限の8割以上の勢いがついているか（合図用）</summary>
    bool IsLaunchReady() const;
    float GetLastLaunchSpeed() const { return lastLaunchSpeed_; }
    const Vector3& GetLastLaunchDirection() const { return lastLaunchDir_; }

    void DrawImGui();

private:
    void StartStance(Player2D* player, Chain2D* chain, const Vector3& socketWorld, MapChip2D* map);
    // 鎖ごと放つ（プレイヤーはまだ飛ばない）
    void Throw(float dt, Player2D* player, Chain2D* chain, const Vector3& socketWorld);
    // 重りの進行方向へプレイヤーを引く
    void Yank(Player2D* player, const Vector3& dir, float weightSpeed);
    // 鎖の実長と壁を考慮した回転半径（angleFromX = +x軸基準の角度、endRadius = お宝の当たり半径分だけ壁から離す）
    float ComputeRadius(MapChip2D* map, Chain2D* chain, const Vector3& center, float angleFromX, float endRadius) const;
    // 振りにくさ：宝石の質量 + 鎖の質量（ユニット数 × chainMassPerUnit_）
    float EffectiveMass(Player2D* player) const;

    ChainParams params_;
    State state_ = State::kIdle;

    // 入力（縁検出）
    bool keyHeld_ = false;
    bool wasHeld_ = false;
    bool pressEdge_ = false;
    bool releaseEdge_ = false;
    float swingInput_ = 0.0f;

    // 振り子状態
    float theta_ = 0.0f;        // 真下を0とした角度（rad。+で右へ振れる）
    float omega_ = 0.0f;        // 角速度（rad/s）
    float radius_ = 0.0f;       // 回転半径
    float effMass_ = 1.0f;      // 現在の振りにくさ（ImGui表示用）
    float launchCap_ = 0.0f;    // 引く速さの上限（構え開始時に決定）
    float cooldownTimer_ = 0.0f;

    // 放った後
    float throwSpeed_ = 0.0f;   // 放った時の重りの速さ（表示用）
    float thrownTimer_ = 0.0f;  // 放ってからの経過秒

    // 直近の引っ張り（ImGui確認用）
    float lastLaunchSpeed_ = 0.0f;
    Vector3 lastLaunchDir_ = { 0.0f, 0.0f, 0.0f };

    // 重り（末端ノード）の拘束先。メンバなのでポインタが安定する
    Vector3 spinTarget_ = { 0.0f, 0.0f, 0.0f };
};
