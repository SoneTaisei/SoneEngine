#pragma once
#include <nlohmann/json.hpp>
#include <set>
#include <string>
#include <utility>
#include <vector>

class MapChip2D;

/// <summary>
/// 収集アイテム（小さい宝石）の進捗。ステージごとに「クリアした時点で取っていた宝石」を記録する
/// - 今回の挑戦で取った分（attempt_）はクリアで記録（saved_）に足す。ミスで最初からになれば消える（シーンを作り直す時に BeginStage で戻す）
/// - 記録は resources/json/local/collectibles.json（ステージのファイル名がキー。座標の一覧）
/// - HUD 用の集計はマップを毎フレーム走査して数える（取った宝石も KeepWhenDestroyed でマップに残るので総数が減らない。
///   破壊フラグは巻き戻し・リプレイで自動復元されるので、それに追従する）
/// </summary>
class CollectibleTracker {
public:
    static CollectibleTracker& Get();

    /// <summary>ステージ開始（GameScene::Initialize。マップを読む前に呼ぶ）。記録を読み、今回の分を空にする</summary>
    void BeginStage(const std::string& mapFilePath);
    const std::string& GetStageKey() const { return stageKey_; }

    /// <summary>以前のクリアで取ったことがある宝石か（薄く表示する用）</summary>
    bool WasCollectedBefore(int chipX, int chipY) const;
    /// <summary>宝石を取った（CollectibleBlock から）</summary>
    void OnCollected(int chipX, int chipY);
    /// <summary>ステージクリア：今回取った分を記録に足して保存する</summary>
    void CommitStageClear();

    struct Entry {
        int x = 0;
        int y = 0;
        bool collectedNow = false;     // 今回の挑戦で取った
        bool collectedBefore = false;  // 以前のクリアで取っている
    };
    struct Summary {
        int total = 0;
        int collectedNow = 0;
        int collectedBefore = 0;
        std::vector<Entry> entries;    // 左から右の順
    };
    /// <summary>マップ上の宝石を数える（HUD・クリア画面用）</summary>
    Summary Summarize(const MapChip2D* map) const;

    /// <summary>取った直後の HUD の跳ね（1 → 0 に減る）</summary>
    float GetPulse() const { return pulse_; }
    void TickPulse(float dt);
    /// <summary>最後に取った宝石の座標（無ければ -1,-1）</summary>
    std::pair<int, int> GetLastCollected() const { return last_; }

private:
    void Load();
    void Save() const;

    std::string stageKey_;
    std::set<std::pair<int, int>> saved_;    // 記録済み（以前のクリアで取った）
    std::set<std::pair<int, int>> attempt_;  // 今回の挑戦で取った
    std::pair<int, int> last_ = { -1, -1 };
    float pulse_ = 0.0f;
    nlohmann::json all_ = nlohmann::json::object(); // 全ステージ分（保存時に他ステージの記録を消さないため）
};
