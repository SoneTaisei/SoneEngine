#pragma once
#include "BaseBlock.h"
#include <algorithm>
#include <vector>

class Object3D;

/// <summary>
/// 警備員
/// 巡回（Patrol）→ 端で待機（Wait）→ プレイヤーを見つけると追跡（Alert）。触れるとミス
/// 鎖で倒す3つの動詞に対応する：
/// - 殴る：宝石（鎖の末端）が一定以上の速さでぶつかると気絶（Stunned）。遅いとよろける（Stagger）だけ
/// - 縛る：気絶中（または背後から）に重なって外すキー → 鎖を1ユニット預けて縛る（Bound）。拾うキーで取り戻すと少しして起きる
/// - 転ばせる：落ちている鎖の節が移動中の足元に重なると短い気絶
/// 気絶・縛られ中は視界が消え、触れてもミスにならない
/// </summary>
class GuardBlock : public BaseBlock {
public:
    enum class State {
        Patrol,
        Wait,
        Alert,       // 追跡（「！」）：最後に見た場所へ走る。見失って loseSightTime_ 経つと調べに移る
        Stunned,     // 気絶（殴られた／転んだ／縛りを解かれた）
        Bound,       // 縛られている（鎖を預かっている）
        Suspicious,  // 疑う（「？」）：チラ見えで立ち止まって向く。すぐ見失えば巡回に戻る
        Investigate, // 調べる（「？」）：最後に見た場所まで歩き、少し見回してから巡回に戻る
    };

    /// <summary>頭上の合図（HUD が描く）</summary>
    enum class Mark { None, Question, Exclamation };

    GuardBlock(MapChip2D* map, int chipX, int chipY);
    ~GuardBlock() override;

    void Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) override;
    void Update() override;
    void Draw() override;

    // 警備員本体は触れるとデスするためソリッド（または独自の当たり判定）
    bool IsSolid() const override { return false; }
    bool IsMoving() const override { return true; }

    Vector3 GetVelocity() const override { return currentVelocity_; }

    void SetProperties(const nlohmann::json& properties) override;
    void Reset() override;

    // リプレイ対応（巡回状態・警戒ゲージ・向きを保存・復元する）
    void CaptureReplayState(std::vector<float>& outCustom) const override;
    void RestoreReplayState(const std::vector<float>& custom) override;

    void OnCollision(Player2D* player) override;

    // 視界領域を取得（気絶・縛られ中は無効な領域を返す）
    AABB2D GetSightAABB() const;

    // プレイヤーが視界に入った時に呼ばれる
    void OnSpottedPlayer(Player2D* player);

    // --- 鎖で倒す ---
    State GetState() const { return state_; }
    /// <summary>気絶か縛られ中（視界なし・触れても安全・動かない）</summary>
    bool IsIncapacitated() const { return state_ == State::Stunned || state_ == State::Bound; }
    /// <summary>移動中か（転ばせるは動いている相手にだけ効く）</summary>
    bool IsMovingState() const { return state_ == State::Patrol || state_ == State::Alert || state_ == State::Investigate; }
    /// <summary>頭上の合図（疑う・調べる = ？、追跡 = ！）</summary>
    Mark GetMark() const {
        if (state_ == State::Alert) return Mark::Exclamation;
        if (state_ == State::Suspicious || state_ == State::Investigate) return Mark::Question;
        return Mark::None;
    }
    /// <summary>頭上の合図を出す位置（頭の少し上）</summary>
    Vector3 GetMarkPosition() const;
    /// <summary>今フレーム視界に入っているか（HUD の「見られている」用）</summary>
    bool IsPlayerInSight() const { return isPlayerInSightThisFrame_; }
    /// <summary>追跡中に見られ続けている割合 0〜1（1 でもう1回「発見」）</summary>
    float GetExposureRatio() const { return (exposureTime_ > 0.0f) ? std::clamp(exposure_ / exposureTime_, 0.0f, 1.0f) : 0.0f; }

    /// <summary>
    /// 宝石がぶつかった。速さが stunSpeed_ 以上なら気絶して true（呼び出し側は宝石の速度を弱める）。
    /// 足りなければよろけるだけで false
    /// </summary>
    bool HitByTreasure(const Vector3& velocity);
    /// <summary>落ちている鎖に足を取られた。移動中でクールダウンが明けていれば短い気絶になり true</summary>
    bool TripByChain(float chainSpeed);
    /// <summary>今この位置のプレイヤーが縛れるか（気絶中、または背後から）</summary>
    bool CanBind(const Vector3& playerPos) const;
    /// <summary>縛る（units ユニットの鎖を預かる）</summary>
    void Bind(int units);
    bool CanUnbind() const { return state_ == State::Bound; }
    /// <summary>縛りを解いて預かっていた鎖のユニット数を返す。少しして起きる</summary>
    int Unbind();
    int GetBoundUnits() const { return boundUnits_; }
    /// <summary>足元の判定（転ばせる用。下 tripFootHeight_ の帯）</summary>
    AABB2D GetFootAABB() const;
    /// <summary>今フレーム、プレイヤーが縛れる／取り戻せる位置にいる（ChainManager が毎フレーム立てる。表示を明るくする合図）</summary>
    void SetPrompt(bool on) { prompt_ = on; }
    /// <summary>見られゲージを 0 に戻す（復活直後の猶予用。追跡中なら巡回に戻る）</summary>
    void ResetAlertGauge() {
        alertGauge_ = 0.0f;
        isPlayerInSightThisFrame_ = false;
        if (state_ == State::Alert) state_ = State::Patrol;
    }

    // エディタの重ね描き用（巡回範囲と初期の向き）
    float GetStartX() const { return startX_; }
    float GetStartY() const { return startY_; }
    float GetMoveRange() const { return moveRange_; }
    int GetStartDirection() const { return startDirection_; }

private:
    void EnterStunned(float duration);
    void UpdateBoundRing();

    float startX_ = 0.0f;
    float startY_ = 0.0f;
    float startWidth_ = 1.0f;
    float startHeight_ = 1.0f;

    Vector3 prevPosition_ = {0.0f, 0.0f, 0.0f};
    Vector3 deltaPosition_ = {0.0f, 0.0f, 0.0f};
    Vector3 currentVelocity_ = {0.0f, 0.0f, 0.0f};

    // パラメータ
    float moveRange_ = 3.0f;      // 片道への最大移動距離
    float patrolSpeed_ = 1.5f;    // パトロール時の速度
    float alertSpeed_ = 3.0f;     // 警戒時の速度
    float sightLength_ = 4.0f;    // 視界の長さ
    float sightHeight_ = 1.0f;    // 視界の高さ
    float maxAlertGauge_ = 1.5f;  // 見つかってからゲームオーバーになるまでの時間(秒)
    int startDirection_ = 1;      // 初期の向き(1:右, -1:左)
    float waitTimeAtEdge_ = 1.0f; // 端に到達した時の待機時間(秒)

    // 鎖で倒すパラメータ（テンプレートの properties で上書き可）
    float stunSpeed_ = 6.0f;      // 宝石がこの速さ以上で当たると気絶（チップ/秒）
    float stunBase_ = 2.0f;       // 気絶の基本時間（秒）
    float stunPerSpeed_ = 0.1f;   // 速さ超過1あたりの気絶延長（秒）
    float stunMax_ = 4.0f;        // 気絶の上限
    float staggerTime_ = 0.2f;    // 不発時のよろけ（止まる）時間
    float wakeWarning_ = 0.5f;    // 起きる前の予告時間（点滅）
    float hitCooldown_ = 0.3f;    // 連続ヒット防止
    float tripStun_ = 1.0f;       // 転んだ時の気絶
    float tripSpeedBonus_ = 0.1f; // 走っているほど長く転ぶ（速さ1あたり秒）
    float tripCooldown_ = 2.0f;   // 転倒の連発防止
    float tripFootHeight_ = 0.3f; // 足元判定の高さ
    float unbindStun_ = 1.5f;     // 縛りを解かれた後の気絶（1秒の猶予の後）
    bool bindFromBehind_ = true;  // 背後から縛れる

    // 見つかるまでの3段階
    float investigateSight_ = 0.5f; // 見られていた時間がこれ以上なら、見失った後に最後の場所を調べに行く（未満なら巡回に戻るだけ）
    float loseSightTime_ = 3.0f;    // 追跡中に見失ってから諦めるまでの秒数
    float lookTime_ = 1.5f;         // 最後に見た場所で見回す秒数
    float exposureTime_ = 2.5f;     // 追跡中に見られ続けてこの秒数で、もう1回「発見」扱い（居座れない）

    // 状態
    State state_ = State::Patrol;
    int direction_ = 1;           // 1: 右, -1: 左
    float currentFacing_ = 1.0f;  // 滑らかな振り向き用（1.0 ~ -1.0）
    float alertGauge_ = 0.0f;
    float waitTimer_ = 0.0f;
    bool isPlayerInSightThisFrame_ = false;
    float stunTimer_ = 0.0f;
    float staggerTimer_ = 0.0f;
    float hitTimer_ = 0.0f;
    float tripTimer_ = 0.0f;
    int boundUnits_ = 0;
    float tumble_ = 0.0f;         // 倒れ角（表示用。0=立ち、±1=横倒し）
    float wobbleTime_ = 0.0f;
    bool prompt_ = false;         // 縛れる／取り戻せる合図（毎フレーム消費）
    float lastSeenX_ = 0.0f;      // 最後にプレイヤーを見た X
    float seenTime_ = 0.0f;       // 今回の視認で見えていた合計秒（疑う→調べるの判定）
    float lostTimer_ = 0.0f;      // 追跡中に見失ってからの秒数
    float lookTimer_ = 0.0f;      // 調べる：見回しの残り秒（到着後）
    bool spottedReported_ = false; // 今回の追跡で発見の加点を済ませた
    float exposure_ = 0.0f;        // 追跡中に見られ続けている秒数（HUD の「！」の下のゲージ）

    // 視界描画用
    std::unique_ptr<GameObject> sightObject_;

    // 縛られている時に体を囲む鎖のリンク（表示専用）
    std::vector<std::unique_ptr<Object3D>> boundLinks_;
    ID3D12Device* device_ = nullptr;
};
