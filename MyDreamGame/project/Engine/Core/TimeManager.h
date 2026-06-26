#pragma once
#include <chrono>

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
    }

    void Update() {
        auto currentTime = std::chrono::steady_clock::now();
        // 生のデルタタイムを計算
        float rawDeltaTime = std::chrono::duration<float>(currentTime - referenceTime_).count();
        referenceTime_ = currentTime;

        // タイムスケールを適用したデルタタイムを計算
        if (useFixedTimeStep_) {
            // TASやリプレイの完全な再現性を保つため、固定フレームレート(60FPS)で更新する
            deltaTime_ = (1.0f / 60.0f) * timeScale_;
        } else {
            deltaTime_ = rawDeltaTime * timeScale_;
        }
    }

    // タイムスケール適用済みのデルタタイムを取得
    float GetDeltaTime() const {
        return deltaTime_;
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
    float deltaTime_ = 0.0f;
    float timeScale_ = 1.0f; // 時間の倍率
    bool useFixedTimeStep_ = true; // デフォルトでON (リプレイ完全同期のため)
};