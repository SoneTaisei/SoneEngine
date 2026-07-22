#pragma once
#include "Core/Utility/Structs.h"
#include <vector>
#include <string>
#include <memory>
#include <random>
#include <cstdint>
#include <thread>
#include <atomic>

class KeyboardInput;
class MapChip2D;

/// <summary>
/// リプレイのヘッダー情報（決定論的システム用）
/// </summary>
struct ReplayHeader {
    uint32_t randomSeed = 1337;          // 録画・再生・検証開始時の乱数シード
    float fixedDeltaTime = 1.0f / 60.0f; // 固定フレームレート
    int totalFrames = 0;
};

/// <summary>
/// 決定論的擬似乱数管理クラス
/// </summary>
class DeterministicRandom {
private:
    std::mt19937 engine;
public:
    DeterministicRandom(uint32_t seed = 1337) {
        engine.seed(seed);
    }
    void Initialize(uint32_t seed) {
        engine.seed(seed);
    }
    int GetRange(int min, int max) {
        std::uniform_int_distribution<int> dist(min, max);
        return dist(engine);
    }
};

/// <summary>
/// ステージ難易度のスコアリング評価結果
/// </summary>
struct DifficultyScore {
    float averageAPM = 0.0f;               // 1分あたりのキー操作切り替え回数 (Actions Per Minute)
    float maxPrecisionScore = 0.0f;        // シリアリティ（崖っぷちジャンプ等の操作精度スコア）
    float stagnationDuration = 0.0f;       // プレイヤーの停滞・迷い時間（秒）
    float finalCalculatedDifficulty = 0.0f; // 総合難易度スコア (0 ~ 100+)
};


/// <summary>
/// 1フレーム分のリプレイデータ
/// </summary>
struct FrameData {
    Vector3 position;      // プレイヤーの座標
    Vector3 cameraPosition;// カメラの座標
    char keys[8];          // "LRJDCWS" のうち押されているものを文字で表した7文字の文字列（+終端文字）。例: "-R-DC--"
    Vector4 color = {1.0f, 1.0f, 1.0f, 1.0f}; // プレイヤーの色
    Vector3 scale = {1.0f, 1.0f, 1.0f};       // プレイヤーのスケール
    Vector3 rotation = {0.0f, 0.0f, 0.0f};    // プレイヤーの回転
};

/// <summary>
/// キーフレーム（ブロック）ごとの動的なブレ設定
/// </summary>
struct JitterSetting {
    int keyIdx;       // どのキーに対する設定か (0~6)
    int startFrame;   // 記録時のブロック開始フレーム
    int endFrame;     // 記録時のブロック終了フレーム
    int maxJitter;    // ブレの強さ（±フレーム）
};

/// <summary>
/// タイムライン上に適用されたマクロの記録
/// </summary>
struct AppliedMacro {
    std::string name;
    int startFrame;
    int duration;
};

/// <summary>
/// 1プレイ全体のリプレイデータ
/// </summary>
struct ReplayData {
    std::string filename;                     // ファイル名（永久保存用）
    std::string dateStr;                      // 録画日時
    std::string stageFilename;                // ★このリプレイを録画した時のマップファイル名
    std::string mapDataStr;                   // ★このリプレイを録画した時の生マップデータ
    Vector3 playerInitPos;                    // プレイヤーの初期座標
    Vector3 cameraInitPos;                    // カメラの初期座標
    int totalFrames = 0;                      // 総フレーム数
    std::vector<FrameData> frames;            // 1フレームずつのデータ (STR)
    std::string mmlTracks[5];                 // MMLに圧縮された5つのキー状態トラック
                                              // T0: 左右移動(L,R,N), T1: ジャンプ(J,N), T2: ダッシュ(D,N), T3: 壁張り付き(C,N), T4: 上下移動(W,S,N)
    std::vector<JitterSetting> jitters;       // 動的ブレ設定リスト
    std::vector<AppliedMacro> appliedMacros;  // 適用されたマクロの記録
    
    int id = -1;                              // このリプレイデータの一意なID
    int parentId = -1;                        // 派生元のリプレイのID（-1ならルート）
};

/// <summary>
/// マクロを構成する1つのブロック（一定フレーム続く入力状態）
/// </summary>
struct MacroBlock {
    int duration = 10;     // 何フレーム続くか
    char keys[8] = "-------"; // その間押されているキー "LRJDCWS" など
};

/// <summary>
/// 登録された入力マクロ
/// </summary>
struct ReplayMacro {
    std::string name;
    std::vector<MacroBlock> blocks;
};

/// <summary>
/// リプレイ、履歴、永久保存システムを管理するシングルトンクラス
/// </summary>
class ReplayManager {
public:
    static ReplayManager* GetInstance();

    // 録画制御
    void StartRecord(const Vector3& initPos, const Vector3& cameraInitPos, const std::string& mapDataStr = "");
    void RecordFrame(const Vector3& pos, const Vector3& cameraPos, const Vector4& color = {1.0f, 1.0f, 1.0f, 1.0f}, const Vector3& scale = {1.0f, 1.0f, 1.0f}, const Vector3& rotation = {0.0f, 0.0f, 0.0f});
    void StopRecord();
    bool PopRecordedFrame(FrameData& outFrame);
    
    // バグ検知時のレポートと自動保存
    void TriggerBugReport(const std::string& reason);


    // 再生制御
    void StartPlayback(int historyIndex = -1, const std::string& filepath = "");
    void StopPlayback();
    void PausePlayback();
    void ResumePlayback();
    
    // 乗っ取り（割り込み）時の特殊な停止処理
    void TakeoverPlayback();
    
    // 選択のみ（再生はしない、残像表示用）
    void SelectReplay(int historyIndex = -1, const std::string& filepath = "");
    
    // 再生更新 (毎フレーム呼び出す、二重発動バグを防ぐ補正ロジックを内包)
    void UpdatePlayback(Vector3& playerPos, Vector3& cameraPos);

    // タイムライン編集時のユーティリティ (編集したSTRからMMLと座標を再生成)
    void ApplyTimelineEdit(int frameIdx, int keyIdx, bool active);
    void SetTrackKeyRange(int trackIdx, int startFrame, int endFrame, bool active);
    void ModifyBlockRange(int trackIdx, int oldStart, int oldEnd, int newStart, int newEnd);
    void DeleteBlockRange(int trackIdx, int startFrame, int endFrame);
    void ApplyMacro(int startFrame, const ReplayMacro& macro); // マクロを流し込む
    void RebuildMmlFromFrames(ReplayData& data);
    void RebuildFramesFromMml(ReplayData& data);

    // ファイルI/O
    bool SaveToFile(const ReplayData& data, const std::string& filename);
    bool LoadFromFile(const std::string& filepath, ReplayData& outData);
    
    // 保存済みファイルリスト管理
    void LoadSavedList();
    void DeleteSavedFile(const std::string& filepath);

    // マクロ管理
    void LoadMacros();
    void SaveMacros();
    std::vector<ReplayMacro>& GetMacros() { return macros_; }
    void AddMacro(const ReplayMacro& macro);
    void RemoveMacro(int index);

    // マクロ録画用
    void ReserveMacroRecording(const std::string& name) { isRecordingMacro_ = true; macroRecordingName_ = name; }
    void CancelMacroRecording() { isRecordingMacro_ = false; macroRecordingName_ = ""; temporaryRecordedFrames_.clear(); }
    bool IsRecordingMacro() const { return isRecordingMacro_; }

    // 高速自動モンキーテスト & 難易度解析 & 物理A*
    void ExecuteFastMonkeyTest(MapChip2D* mapChip, int iterations = 10, int jitterChance = 5);
    DifficultyScore AnalyzeReplayDifficulty(const std::vector<FrameData>& replayData, MapChip2D* mapChip);
    std::vector<FrameData> SimulateMacro(const std::vector<FrameData>& perfectMacro, MapChip2D* mapChip);
    class LevelEvolutionAI* GetLevelEvolutionAI() { return levelEvolutionAI_.get(); }

    // 物理ベースA* 探索ルート (非同期スレッド対応)
    void ExecuteAStarAsync(const Vector3& startPos, const Vector3& goalPos, MapChip2D* mapChip, int maxNodes = 10000);
    bool IsAISearching() const { return isAISearching_.load(); }

    void SetAIPathPositions(const std::vector<Vector3>& path) { aiPathPositions_ = path; }
    const std::vector<Vector3>& GetAIPathPositions() const { return aiPathPositions_; }
    void ClearAIPathPositions() { aiPathPositions_.clear(); }
    
    bool IsShowAIGhost() const { return showAIGhost_; }
    void SetShowAIGhost(bool show) { showAIGhost_ = show; }

    const std::string& GetAIPathStatusMsg() const { return aiPathStatusMsg_; }
    void SetAIPathStatusMsg(const std::string& msg) { aiPathStatusMsg_ = msg; }

    uint32_t GetRandomSeed() const { return replayHeader_.randomSeed; }
    void SetRandomSeed(uint32_t seed) { replayHeader_.randomSeed = seed; deterministicRandom_.Initialize(seed); }
    DeterministicRandom& GetDeterministicRandom() { return deterministicRandom_; }
    const ReplayHeader& GetReplayHeader() const { return replayHeader_; }

    const std::vector<std::string>& GetMonkeyTestLogs() const { return monkeyTestLogs_; }
    void ClearMonkeyTestLogs() { monkeyTestLogs_.clear(); }
    const DifficultyScore& GetLastAnalyzedScore() const { return lastAnalyzedScore_; }

    // ゲッター・セッター
    bool IsRecording() const { return isRecording_; }
    bool IsPlaying() const { return isPlaying_; }
    bool IsPaused() const { return isPaused_; }
    int GetCurrentFrame() const { return currentFrame_; }
    int GetRecordedFrameCount() const { return static_cast<int>(temporaryRecordedFrames_.size()); }
    void SetCurrentFrame(int frame); // シーク用
    bool IsForceSnapNextFrame() const { return forceSnapNextFrame_; }

    bool IsLoopPlay() const { return isLoopPlay_; }
    void SetLoopPlay(bool enable) { isLoopPlay_ = enable; }

    bool IsSnapEnabled() const { return isSnapEnabled_; }
    void SetSnapEnabled(bool enable) { isSnapEnabled_ = enable; }

    bool IsInterpolationEnabled() const { return isInterpolationEnabled_; }
    void SetInterpolationEnabled(bool enable) { isInterpolationEnabled_ = enable; }

    ReplayData& GetCurrentReplay() { return currentReplay_; }
    const std::vector<ReplayData>& GetHistory() const { return history_; }
    const std::vector<std::string>& GetSavedList() const { return savedList_; }

    void SetCurrentStageFilename(const std::string& filename) { currentStageFilename_ = filename; }
    const std::string& GetCurrentMapDataStr() const { return currentMapDataStr_; }
    const std::vector<FrameData>& GetTemporaryRecordedFrames() const { return temporaryRecordedFrames_; }

private:
    ReplayManager();
    ~ReplayManager();
    ReplayManager(const ReplayManager&) = delete;
    ReplayManager& operator=(const ReplayManager&) = delete;

    // 再生・ループ時に実行用キーバッファを生成する
    void GenerateRuntimeKeys();
    bool CheckCollisionAt(float x, float y, MapChip2D* mapChip) const;

private:
    ReplayHeader replayHeader_;                       // 決定論的ヘッダー情報
    DeterministicRandom deterministicRandom_{ 1337 }; // 決定論的乱数発生器
    std::vector<std::string> monkeyTestLogs_;          // モンキーテストの実行ログ
    DifficultyScore lastAnalyzedScore_;               // 直近の難易度解析スコア
    std::vector<Vector3> aiPathPositions_;            // 物理A*で計算されたAI探索ルート
    bool showAIGhost_ = true;                         // AIゴースト描画フラグ
    std::string aiPathStatusMsg_ = "";                // AI探索結果ステータスメッセージ
    std::thread aiSearchThread_;                      // 非同期AI探索用スレッド
    std::atomic<bool> isAISearching_{ false };         // AI探索中フラグ

    bool isRecording_ = false;
    bool isPlaying_ = false;
    bool isPaused_ = false;
    bool isLoopPlay_ = false;
    bool isSnapEnabled_ = true;
    bool isInterpolationEnabled_ = true; // フレーム間の座標補間フラグ
    bool forceSnapNextFrame_ = false; // シーク時に強制スナップするフラグ
    bool hasLoggedDesync_ = false;    // 再生中にズレをすでにログ出力したかどうか
    bool isTakeoverRecording_ = false; // 乗っ取り（割り込み）からの録画フラグ
    bool isRecordingMacro_ = false; // マクロを録画するかどうかのフラグ
    std::string macroRecordingName_ = ""; // マクロ録画時の保存名
    int takeoverFrame_ = 0;           // 乗っ取りが発生したフレーム
    int takeoverSourceId_ = -1;       // 乗っ取り元のReplayData ID
    int nextReplayId_ = 1;            // 次に割り当てるReplayData ID
    int currentFrame_ = 0;


    Vector3 playerInitPos_ = { 0.0f, 0.0f, 0.0f };
    Vector3 cameraInitPos_ = { 0.0f, 0.0f, 0.0f };
    std::string currentStageFilename_ = "";           // 録画時に参照するマップファイル名
    std::string currentMapDataStr_ = "";              // 録画時に参照する生マップデータ
    std::vector<FrameData> temporaryRecordedFrames_; // 録画中の一時バッファ

    ReplayData currentReplay_;                        // 現在アクティブなリプレイ（再生用・編集用）
    std::vector<ReplayData> history_;                 // 直近3回のプレイ履歴（0: 最新, 1: 1回前, 2: 2回前）
    std::vector<std::string> savedList_;              // json/saved_replays/ 下のファイル名リスト
    std::vector<ReplayMacro> macros_;                 // 登録されたマクロリスト
    
    std::unique_ptr<class LevelEvolutionAI> levelEvolutionAI_;
    std::vector<std::string> runtimeKeys_;            // 再生時に使用する動的キー配列 (1要素につき7文字の文字列)
};
