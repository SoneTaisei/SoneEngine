#pragma once
#include <chrono>

// =========================================================================
// 時間管理クラス（シングルトン本体）
// =========================================================================
class TimeManager {
public:
    static TimeManager &GetInstance() {
        static TimeManager instance;
        return instance;
    }

    TimeManager(const TimeManager &) = delete;
    TimeManager &operator=(const TimeManager &) = delete;
    TimeManager(TimeManager &&) = delete;
    TimeManager &operator=(TimeManager &&) = delete;

    void Initialize() {
        referenceTime_ = std::chrono::steady_clock::now();
        timeScale_ = 1.0f; // タイムスケールを初期化
        deltaTime_ = 0.0f;
        rawDeltaTime_ = 0.0f;
        gameDeltaTime_ = 0.0f;
        isPaused_ = false;
    }

    void Update() {
        auto currentTime = std::chrono::steady_clock::now();
        // 1. 現実の実時間を計算
        rawDeltaTime_ = std::chrono::duration<float>(currentTime - referenceTime_).count();
        referenceTime_ = currentTime;

        // 2. スローモーション適用（固定60FPS or 可変FPS）
        float baseDt = useFixedTimeStep_ ? (1.0f / 60.0f) : rawDeltaTime_;
        deltaTime_ = baseDt * timeScale_;

        // 3. ゲーム用デルタタイムをここで確定させておく！
        // （ポーズ中なら 0.0f、動いているならスロー適用後の時間）
        if (isPaused_) {
            gameDeltaTime_ = 0.0f;
        } else {
            gameDeltaTime_ = deltaTime_;
        }
    }

    // ゲーム用デルタタイムを取得（ポーズ中は0.0f、スローモーション対応）
    float GetGameDeltaTime() const {
        return gameDeltaTime_;
    }

    // 実時間のデルタタイムを取得（ポーズやスローモーションの影響を受けない）
    float GetUnscaledDeltaTime() const {
        return rawDeltaTime_;
    }

    // タイムスケール適用済みの生デルタタイムを取得（互換用）
    float GetDeltaTime() const {
        return deltaTime_;
    }

    // ポーズ状態の切り替え
    void SetPaused(bool paused) {
        isPaused_ = paused;
    }
    bool IsPaused() const {
        return isPaused_;
    }

    // タイムスケールを設定する
    void SetTimeScale(float scale) {
        timeScale_ = scale;
    }

    // 現在のタイムスケールを取得する
    float GetTimeScale() const {
        return timeScale_;
    }

    // 固定フレームレートモードのON/OFF
    void SetUseFixedTimeStep(bool enable) {
        useFixedTimeStep_ = enable;
    }
    bool IsFixedTimeStep() const {
        return useFixedTimeStep_;
    }

private:
    TimeManager() = default;
    ~TimeManager() = default;

    std::chrono::steady_clock::time_point referenceTime_;
    float rawDeltaTime_ = 0.0f;    // 現実の実経過時間
    float deltaTime_ = 0.0f;       // スロー適用後の時間
    float gameDeltaTime_ = 0.0f;   // ポーズ判定済みのゲーム用時間
    float timeScale_ = 1.0f;       // 時間の倍率
    bool isPaused_ = false;        // 一時停止状態
    bool useFixedTimeStep_ = true; // デフォルトでON (リプレイ完全同期のため)
};

// =========================================================================
// どこからでも1行で呼べる静的クラス（UnityのTimeクラスと同じ使い勝手）
// =========================================================================
class Time {
public:
    // ゲーム用デルタタイム（ポーズ中は0.0f、スローモーション対応）
    static float GetDeltaTime() {
        return TimeManager::GetInstance().GetGameDeltaTime();
    }
    static float DeltaTime() { return GetDeltaTime(); } // 短縮用

    // 実時間のデルタタイム（ポーズやスローの影響を受けない）
    static float GetUnscaledDeltaTime() {
        return TimeManager::GetInstance().GetUnscaledDeltaTime();
    }
    static float UnscaledDeltaTime() { return GetUnscaledDeltaTime(); } // 短縮用

    // 物理・リプレイ用の固定デルタタイム（厳密に1/60秒）
    static constexpr float GetFixedDeltaTime() {
        return 1.0f / 60.0f;
    }
    static constexpr float FixedDeltaTime() { return GetFixedDeltaTime(); }

    // ポーズ操作
    static void SetPaused(bool paused) {
        TimeManager::GetInstance().SetPaused(paused);
    }
    static bool IsPaused() {
        return TimeManager::GetInstance().IsPaused();
    }

    // タイムスケール操作（スローモーション・倍速）
    static void SetTimeScale(float scale) {
        TimeManager::GetInstance().SetTimeScale(scale);
    }
    static float GetTimeScale() {
        return TimeManager::GetInstance().GetTimeScale();
    }
};