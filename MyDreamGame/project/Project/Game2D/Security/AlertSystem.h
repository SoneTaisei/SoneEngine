#pragma once
#include <map>
#include <string>
#include <vector>
#include "Core/Utility/Structs.h"
#include "Editor/Replay/ReplayManager.h"

class MapChip2D;

/// <summary>
/// 警戒度のパラメータ（ParameterManager の "Alert" グループ）
/// </summary>
struct AlertParams {
    // ---- 回数制（既定）：追跡に入った回数が上限に達したら捕獲。回復なし ----
    bool strikeEnabled_ = true;      // 回数制を使うか
    int strikeLimit_ = 3;            // 見つかっていい回数（この回数目の「！」で捕獲）
    float strikeMergeTime_ = 1.0f;   // この秒数以内の同時発見は1回にまとめる

    // ---- 値の警戒度（ハードモード用。既定 OFF）----
    bool enabled_ = false;           // 警戒度を使うか。OFF なら値は動かず HUD も出ない（警備員だけで完結する方針）
    float seenPerSec_ = 8.0f;        // 警備員の視界に入っている間（ゲージが溜まっている間）の毎秒加算
    float spottedAdd_ = 25.0f;       // 発見確定（ゲージ満タン）
    float wakeAdd_ = 15.0f;          // 気絶した警備員が起きる（通報）
    bool noiseEnabled_ = false;      // 騒音を数えるか（宝石を引きずるだけで上がり続けるので、いったん OFF）
    float noiseAdd_ = 5.0f;          // 騒音（宝石の着地・床が崩れる）
    float noiseRadius_ = 6.0f;       // 騒音が届く距離（チップ）。この中に警備員がいる時だけ加算
    float noiseSpeed_ = 8.0f;        // 宝石がこの速さ以上で当たると騒音
    float driftPerSec_ = 1.0f;       // 時間経過（無策の上限）
    float quietDelay_ = 8.0f;        // これだけ静かだと下がり始める（秒）
    float quietDecayPerSec_ = 2.0f;  // 静かな時の毎秒減少
    float captureValue_ = 100.0f;    // ここに達したら捕獲
    float wakeFarDistance_ = 8.0f;   // 起きた時プレイヤーがこれ以上離れていれば通報が弱い（チップ）
    float wakeFarAdd_ = 5.0f;        // 遠くで起きた時の通報
    float respawnGrace_ = 3.0f;      // 復活直後の猶予（秒）。時間経過と加算を止める
};

/// <summary>クリア時の評価</summary>
struct AlertRank {
    char rank = 'C';       // S / A / B / C
    int spotted = 0;       // 発見された回数
    int reported = 0;      // 通報された回数
    int noises = 0;        // 騒音の回数
    float peak = 0.0f;     // 最大警戒度
};

/// <summary>
/// 警戒度（最小構成）：0〜100 の値を1つ持ち、満タンで捕獲（ステージ失敗）
/// GameScene が所有し、警備員・鎖・崩れる床は AlertSystem::Current() から事象を足す
/// 値は表示と捕獲判定にだけ使い、警備員の視界や速度には掛けない
/// </summary>
class AlertSystem : public IReplayObjectProvider {
public:
    struct Event {
        std::string text;   // 「発見 +25」など
        float age = 0.0f;   // 起きてからの秒数（ポップアップ用）
        bool good = false;  // 見返り（緑で出す。値は変わらない）
    };

    AlertSystem();
    ~AlertSystem() override;

    /// <summary>今動いている警戒度（GameScene の間だけ。無ければ nullptr）</summary>
    static AlertSystem* Current() { return s_current; }
    void SetAsCurrent(bool on);

    void LoadParams();
    void SaveParams();
    const AlertParams& GetParams() const { return params_; }

    /// <summary>ステージ開始・リトライ。値を 0 に戻す（死亡のリスポーンでは呼ばない）</summary>
    void Reset();
    /// <summary>Playing 中だけ true。false の間は事象を受け付けない（StartReady / Clear / 捕獲後 / エディタ中）</summary>
    void SetActive(bool on) { active_ = on; }
    bool IsActive() const { return active_; }
    /// <summary>毎フレーム（Playing 中だけ）。時間経過の加算、静かな時の減衰、捕獲判定</summary>
    void Update(float dt);

    /// <summary>事象（発見・通報・騒音）。ポップアップを出し、静音タイマーを戻す</summary>
    void Add(float amount, const char* reason);
    /// <summary>視界に入っている間の毎秒加算（ポップアップ無し。静音タイマーは戻す）</summary>
    void AddContinuous(float amountPerSec, float dt);
    /// <summary>騒音。pos から noiseRadius_ 以内に（気絶していない）警備員がいる時だけ noiseAdd_ を足す。足したら true</summary>
    bool AddNoise(const Vector3& pos, MapChip2D* map, const char* reason);
    /// <summary>プレイヤーが視界に入っている、を毎フレーム受ける（静音判定用。見失ってゲージが抜けている間は数えない）</summary>
    void NotifyGuardAlert() { guardAlertThisFrame_ = true; }
    /// <summary>見返りの合図（「通報 回避」など）。値は変えず緑のポップアップだけ出す</summary>
    void Notice(const char* text);
    /// <summary>気絶した警備員が起きた。プレイヤーが遠ければ通報が弱い</summary>
    void OnGuardWake(const Vector3& guardPos);
    /// <summary>警備員が追跡に入った（発見確定）。回数制なら1回数え、上限で捕獲。値の警戒度なら +spottedAdd_</summary>
    void OnSpotted();
    /// <summary>追跡中に見られ続けた（居座り）。まとめ判定を通さずにもう1回「発見」扱い</summary>
    void OnExposed();
    int GetStrikes() const { return strikes_; }
    int GetStrikeLimit() const { return params_.strikeLimit_; }
    /// <summary>発見の直後 1 → 0（HUD のアイコンを跳ねさせる）</summary>
    float GetStrikePulse() const { return strikePulse_; }
    /// <summary>復活直後の猶予を始める（時間経過と加算を止める）</summary>
    void StartGrace(float seconds);
    bool IsInGrace() const { return graceTimer_ > 0.0f; }
    float GetGraceTimer() const { return graceTimer_; }
    /// <summary>今フレーム見られている（HUD の合図用。少しの間 true が続く）</summary>
    bool IsBeingSeen() const { return seenTimer_ > 0.0f; }
    /// <summary>プレイヤー位置（通報の距離判定用。GameScene が毎フレーム渡す）</summary>
    void SetPlayerPosition(const Vector3& pos) { playerPos_ = pos; }
    /// <summary>クリア時の評価（発見・通報・騒音の回数と最大警戒度から）</summary>
    AlertRank ComputeRank() const;
    float GetPeak() const { return peak_; }

    float GetValue() const { return value_; }
    float GetRatio() const { return (params_.captureValue_ > 0.0f) ? value_ / params_.captureValue_ : 0.0f; }
    bool IsCaptured() const { return captured_; }
    /// <summary>値が動いた直後 1 → 0 に戻る（HUD のバーを少し大きくする用）</summary>
    float GetPulse() const { return pulse_; }
    /// <summary>直近の事象（HUD のポップアップ用。age は秒）</summary>
    const std::vector<Event>& GetEvents() const { return events_; }
    /// <summary>事象ごとの合計（後でランクや統計に使う）</summary>
    const std::map<std::string, float>& GetTotals() const { return totals_; }

    // ===== IReplayObjectProvider（シーク用に value_ / quietTimer_ / captured_ を記録） =====
    const char* GetReplayProviderName() const override { return "AlertSystem"; }
    void CaptureReplayObjects(std::vector<ReplayObjectState>& out) override;
    void RestoreReplayObjects(const std::vector<ReplayObjectState>& states) override;

    void DrawImGui();

private:
    void Clamp();

    static AlertSystem* s_current;

    AlertParams params_;
    float value_ = 0.0f;
    float quietTimer_ = 0.0f;
    bool captured_ = false;
    bool active_ = false;
    bool guardAlertThisFrame_ = false;
    bool eventThisFrame_ = false;
    float pulse_ = 0.0f;
    int strikes_ = 0;
    float strikeTime_ = -100.0f;     // 最後に数えた発見の時刻（まとめる用）
    float clock_ = 0.0f;             // Update で進む内部時計
    float strikePulse_ = 0.0f;
    float graceTimer_ = 0.0f;
    float seenTimer_ = 0.0f;
    float peak_ = 0.0f;
    Vector3 playerPos_ = {0.0f, 0.0f, 0.0f};
    std::vector<Event> events_;
    std::map<std::string, float> totals_;
    std::map<std::string, int> counts_;
};
