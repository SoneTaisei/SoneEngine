#pragma once
#include "Core/Utility/Structs.h"
#include <vector>
#include <string>
#include <memory>

class KeyboardInput;

/// <summary>
/// 1フレーム分のリプレイデータ
/// </summary>
struct FrameData {
    Vector3 position;      // プレイヤーの座標
    Vector3 cameraPosition;// カメラの座標
    char keys[6];          // "LRJDC" のうち押されているものを文字で表した5文字の文字列（+終端文字）。例: "-R-D-"
};

/// <summary>
/// 1プレイ全体のリプレイデータ
/// </summary>
struct ReplayData {
    std::string filename;                     // ファイル名（永久保存用）
    std::string dateStr;                      // 録画日時
    Vector3 playerInitPos;                    // プレイヤーの初期座標
    Vector3 cameraInitPos;                    // カメラの初期座標
    int totalFrames = 0;                      // 総フレーム数
    std::vector<FrameData> frames;            // 1フレームずつのデータ (STR)
    std::string mmlTracks[4];                 // MMLに圧縮された4つのキー状態トラック
                                              // T0: 左右移動(L,R,N), T1: ジャンプ(J,N), T2: ダッシュ(D,N), T3: 壁張り付き(C,N)
};

/// <summary>
/// リプレイ、履歴、永久保存システムを管理するシングルトンクラス
/// </summary>
class ReplayManager {
public:
    static ReplayManager* GetInstance();

    // 録画制御
    void StartRecord(const Vector3& initPos, const Vector3& cameraInitPos);
    void RecordFrame(const Vector3& pos, const Vector3& cameraPos);
    void StopRecord();

    // 再生制御
    void StartPlayback(int historyIndex = -1, const std::string& filepath = "");
    void StopPlayback();
    void PausePlayback();
    void ResumePlayback();
    
    // 再生更新 (毎フレーム呼び出す、二重発動バグを防ぐ補正ロジックを内包)
    void UpdatePlayback(Vector3& playerPos, Vector3& cameraPos);

    // タイムライン編集時のユーティリティ (編集したSTRからMMLと座標を再生成)
    void ApplyTimelineEdit(int frameIdx, int keyIdx, bool active);
    void RebuildMmlFromFrames(ReplayData& data);
    void RebuildFramesFromMml(ReplayData& data);

    // ファイルI/O
    bool SaveToFile(const ReplayData& data, const std::string& filename);
    bool LoadFromFile(const std::string& filepath, ReplayData& outData);
    
    // 保存済みファイルリスト管理
    void LoadSavedList();
    void DeleteSavedFile(const std::string& filepath);

    // ゲッター・セッター
    bool IsRecording() const { return isRecording_; }
    bool IsPlaying() const { return isPlaying_; }
    bool IsPaused() const { return isPaused_; }
    int GetCurrentFrame() const { return currentFrame_; }
    int GetRecordedFrameCount() const { return static_cast<int>(temporaryRecordedFrames_.size()); }
    void SetCurrentFrame(int frame); // シーク用

    bool IsLoopPlay() const { return isLoopPlay_; }
    void SetLoopPlay(bool enable) { isLoopPlay_ = enable; }

    ReplayData& GetCurrentReplay() { return currentReplay_; }
    const std::vector<ReplayData>& GetHistory() const { return history_; }
    const std::vector<std::string>& GetSavedList() const { return savedList_; }

private:
    ReplayManager() = default;
    ~ReplayManager() = default;
    ReplayManager(const ReplayManager&) = delete;
    ReplayManager& operator=(const ReplayManager&) = delete;

    // MMLエンコード/デコード処理の内部関数
    std::string EncodeTrackToMml(const std::vector<char>& rawTrack);
    std::vector<char> DecodeMmlToTrack(const std::string& mmlStr, int expectedFrames);

private:
    bool isRecording_ = false;
    bool isPlaying_ = false;
    bool isPaused_ = false;
    bool isLoopPlay_ = false;
    int currentFrame_ = 0;

    Vector3 playerInitPos_ = { 0.0f, 0.0f, 0.0f };
    Vector3 cameraInitPos_ = { 0.0f, 0.0f, 0.0f };
    std::vector<FrameData> temporaryRecordedFrames_; // 録画中の一時バッファ

    ReplayData currentReplay_;                        // 現在アクティブなリプレイ（再生用・編集用）
    std::vector<ReplayData> history_;                 // 直近3回のプレイ履歴（0: 最新, 1: 1回前, 2: 2回前）
    std::vector<std::string> savedList_;              // json/saved_replays/ 下のファイル名リスト
};
