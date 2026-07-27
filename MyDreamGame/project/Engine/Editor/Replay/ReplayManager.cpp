#include "ReplayManager.h"
#include "Game2D/MapChip2D.h"
#include "PhysicsAStar.h"
#include "LevelEvolutionAI.h"
#include "ReplayIO.h"
#include "ReplayTimelineEditor.h"
#include "ReplayTester.h"
#include "Core/Utility/LogManager.h"
#include "Input/KeyboardInput.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <ctime>
#include <iomanip>
#include <algorithm>
#include <cmath>
#include <cstdlib>

ReplayManager* ReplayManager::GetInstance() {
    static ReplayManager instance;
    return &instance;
}

ReplayManager::ReplayManager() {
    levelEvolutionAI_ = std::make_unique<LevelEvolutionAI>();
}

ReplayManager::~ReplayManager() {
    if (aiSearchThread_.joinable()) {
        aiSearchThread_.join();
    }
}

void ReplayManager::LoadMacros() {
    macros_ = ReplayIO::LoadMacros();
}

void ReplayManager::SaveMacros() {
    ReplayIO::SaveMacros(macros_);
}

void ReplayManager::AddMacro(const ReplayMacro& macro) {
    macros_.push_back(macro);
    SaveMacros();
}

void ReplayManager::RemoveMacro(int index) {
    if (index >= 0 && index < static_cast<int>(macros_.size())) {
        macros_.erase(macros_.begin() + index);
        SaveMacros();
    }
}

void ReplayManager::ApplyMacro(int startFrame, const ReplayMacro& macro) {
    ReplayTimelineEditor::ApplyMacro(currentReplay_, startFrame, macro);
}

void ReplayManager::StartRecord(const Vector3& initPos, const Vector3& cameraInitPos, const std::string& mapDataStr) {
    if (isRecording_) return;
    
    isRecording_ = true;
    isPlaying_ = false;
    isPaused_ = false;
    
    if (isTakeoverRecording_) {
        // テイクオーバー時：元のリプレイから初期情報を引き継ぎ、過去のフレームデータをコピーする
        playerInitPos_ = currentReplay_.playerInitPos;
        cameraInitPos_ = currentReplay_.cameraInitPos;
        currentMapDataStr_ = currentReplay_.mapDataStr;
        currentStageFilename_ = currentReplay_.stageFilename;
        
        temporaryRecordedFrames_.clear();
        int endFrame = (std::min)(takeoverFrame_, currentReplay_.totalFrames - 1);
        for (int i = 0; i <= endFrame; ++i) {
            temporaryRecordedFrames_.push_back(currentReplay_.frames[i]);
        }
        
        // フレームカウンタは引き継いだ分だけ進めた状態にする（必要なら）
        // ただし録画時は currentFrame_ は使わず temporaryRecordedFrames_.size() が参照される
        currentFrame_ = takeoverFrame_; 
        isTakeoverRecording_ = false; // フラグをリセット
    } else {
        // 通常の録画時
        currentFrame_ = 0;
        playerInitPos_ = initPos;
        cameraInitPos_ = cameraInitPos;
        currentMapDataStr_ = mapDataStr;
        temporaryRecordedFrames_.clear();
    }
    
    // 注入モードがオンになっていればオフにする
    KeyboardInput::GetInstance()->SetReplayMode(false);
}

void ReplayManager::RecordFrame(const Vector3& pos, const Vector3& cameraPos, const Vector4& color, const Vector3& scale, const Vector3& rotation) {
    if (!isRecording_) return;

    KeyboardInput* keyboard = KeyboardInput::GetInstance();
    FrameData frame;
    frame.position = pos;
    frame.cameraPosition = cameraPos;
    frame.color = color;
    frame.scale = scale;
    frame.rotation = rotation;

    // "LRJDCWS" の初期文字列
    frame.keys[0] = (keyboard->IsKeyDown(DIK_A) || keyboard->IsKeyDown(DIK_LEFT)) ? 'L' : '-';
    frame.keys[1] = (keyboard->IsKeyDown(DIK_D) || keyboard->IsKeyDown(DIK_RIGHT)) ? 'R' : '-';
    frame.keys[2] = keyboard->IsKeyDown(DIK_SPACE) ? 'J' : '-';
    frame.keys[3] = (keyboard->IsKeyDown(DIK_LSHIFT) || keyboard->IsKeyDown(DIK_RSHIFT)) ? 'D' : '-';
    frame.keys[4] = (keyboard->IsKeyDown(DIK_LCONTROL) || keyboard->IsKeyDown(DIK_RCONTROL)) ? 'C' : '-';
    frame.keys[5] = (keyboard->IsKeyDown(DIK_W) || keyboard->IsKeyDown(DIK_UP)) ? 'W' : '-';
    frame.keys[6] = (keyboard->IsKeyDown(DIK_S) || keyboard->IsKeyDown(DIK_DOWN)) ? 'S' : '-';
    frame.keys[7] = '\0';

    temporaryRecordedFrames_.push_back(frame);
}

void ReplayManager::StopRecord() {
    if (!isRecording_) return;
    isRecording_ = false;

    bool wasMacroRecording = isRecordingMacro_;

    // マクロ録画予約されていた場合、録画した入力データをマクロとして抽出
    if (isRecordingMacro_ && !temporaryRecordedFrames_.empty()) {
        ReplayMacro rm;
        rm.name = macroRecordingName_.empty() ? "RecordedMacro" : macroRecordingName_;
        
        MacroBlock currentBlock;
        bool isFirst = true;
        
        for (const auto& frame : temporaryRecordedFrames_) {
            char currentKeys[8];
            for(int k=0; k<7; ++k) currentKeys[k] = frame.keys[k];
            currentKeys[7] = '\0';
            
            if (isFirst) {
                currentBlock.duration = 1;
                strncpy_s(currentBlock.keys, currentKeys, sizeof(currentBlock.keys));
                isFirst = false;
            } else {
                bool same = true;
                for(int k=0; k<7; ++k) {
                    if(currentBlock.keys[k] != currentKeys[k]) { same = false; break; }
                }
                if (same) {
                    currentBlock.duration++;
                } else {
                    rm.blocks.push_back(currentBlock);
                    currentBlock.duration = 1;
                    strncpy_s(currentBlock.keys, currentKeys, sizeof(currentBlock.keys));
                }
            }
        }
        if (!isFirst) {
            rm.blocks.push_back(currentBlock);
        }
        if (rm.blocks.empty()) rm.blocks.push_back({10, "-------"});
        
        AddMacro(rm);
        isRecordingMacro_ = false;
    }

    if (wasMacroRecording) {
        temporaryRecordedFrames_.clear();
        return;
    }

    // 録画されたフレーム数が極端に短い場合は履歴に登録しない
    if (temporaryRecordedFrames_.size() < 5) {
        temporaryRecordedFrames_.clear();
        return;
    }

    ReplayData data;
    data.playerInitPos = playerInitPos_;
    data.cameraInitPos = cameraInitPos_;
    data.stageFilename = currentStageFilename_;
    data.mapDataStr = currentMapDataStr_;
    data.totalFrames = static_cast<int>(temporaryRecordedFrames_.size());
    data.frames = temporaryRecordedFrames_;

    // 日時の取得
    auto now = std::time(nullptr);
    struct tm timeinfo;
    localtime_s(&timeinfo, &now);
    std::stringstream ss;
    ss << std::put_time(&timeinfo, "%Y/%m/%d %H:%M:%S");
    data.dateStr = ss.str();
    
    // ツリー構造のためのID付与
    data.id = nextReplayId_++;
    data.parentId = takeoverSourceId_;
    takeoverSourceId_ = -1; // 録画が終わったらリセット

    // MMLトラックへの圧縮
    RebuildMmlFromFrames(data);

    // 履歴（直近10回分）のリングバッファ更新
    if (history_.size() >= 10) {
        history_.pop_back();
    }
    history_.insert(history_.begin(), data);

    temporaryRecordedFrames_.clear();
}

bool ReplayManager::PopRecordedFrame(FrameData& outFrame) {
    if (temporaryRecordedFrames_.empty()) {
        return false;
    }
    outFrame = temporaryRecordedFrames_.back();
    temporaryRecordedFrames_.pop_back();
    return true;
}

#include "Core/Utility/LogManager.h"
#include <chrono>
#include <format>
#include <ctime>

void ReplayManager::TriggerBugReport(const std::string& reason) {
    LogManager::GetInstance()->AddLog(LogLevel::Error, "[Bug Report] " + reason);
    
    // 現在のリプレイ状態を自動保存
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    localtime_s(&tm, &time);
    std::string filename = std::format("bug_report_{:04}{:02}{:02}_{:02}{:02}{:02}.mml", 
                                        tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, 
                                        tm.tm_hour, tm.tm_min, tm.tm_sec);
                                        
    // 録画中なら履歴に残すために一旦StopRecordを呼んで保存するか、現在の履歴を保存する
    if (isRecording_) {
        StopRecord();
    }
    
    if (history_.size() > 0) {
        // 直前のプレイ（もしくは現在のバグったプレイ）を保存
        SaveToFile(history_[0], filename);
    } else {
        SaveToFile(currentReplay_, filename);
    }
    
    LogManager::GetInstance()->AddLog(LogLevel::Info, "Bug report replay saved to: " + filename);
    
    // 安全に停止
    StopPlayback();
}

void ReplayManager::StartPlayback(int historyIndex, const std::string& filepath) {
    if (historyIndex >= 0 && historyIndex < static_cast<int>(history_.size())) {
        currentReplay_ = history_[historyIndex];
    } else if (!filepath.empty()) {
        if (!LoadFromFile(filepath, currentReplay_)) {
            return;
        }
    } else {
        // 引数が指定されていない場合は、現在の編集中のリプレイ（currentReplay_）をそのまま使う
        if (currentReplay_.totalFrames == 0) {
            return;
        }
    }

    if (currentReplay_.totalFrames == 0) return;

    isPlaying_ = true;
    isPaused_ = false;
    isRecording_ = false;
    currentFrame_ = 0;
    hasLoggedDesync_ = false;
    forceSnapNextFrame_ = true; // 再生開始時に強制的に状態を再構築させる

    // KeyboardInput をリプレイモードに切り替える
    KeyboardInput::GetInstance()->SetReplayMode(true);
    
    // 実行用キーバッファを生成（ランダムブレの適用）
    GenerateRuntimeKeys();
}

void ReplayManager::SelectReplay(int historyIndex, const std::string& filepath) {
    // 再生中や録画中なら停止する
    if (isPlaying_) StopPlayback();
    if (isRecording_) StopRecord();

    if (historyIndex >= 0 && historyIndex < static_cast<int>(history_.size())) {
        currentReplay_ = history_[historyIndex];
    } else if (!filepath.empty()) {
        LoadFromFile(filepath, currentReplay_);
    }
    
    // 再生は開始せず、フレームを0にしておく
    currentFrame_ = 0;
    hasLoggedDesync_ = false;
}

void ReplayManager::StopPlayback() {
    if (!isPlaying_) return;
    isPlaying_ = false;
    isPaused_ = false;
    currentFrame_ = 0;
    hasLoggedDesync_ = false;

    // KeyboardInput を通常モードに戻す
    KeyboardInput::GetInstance()->SetReplayMode(false);
}

void ReplayManager::TakeoverPlayback() {
    if (!isPlaying_) return;
    isPlaying_ = false;
    isPaused_ = false;
    isTakeoverRecording_ = true;
    takeoverFrame_ = currentFrame_;
    takeoverSourceId_ = currentReplay_.id; // 派生元のIDを記憶
    
    // currentFrame_ は0にリセットせず、KeyboardInput を通常モードに戻す
    KeyboardInput::GetInstance()->SetReplayMode(false);
}

void ReplayManager::PausePlayback() {
    if (isPlaying_) {
        isPaused_ = true;
    }
}

void ReplayManager::ResumePlayback() {
    if (isPlaying_) {
        isPaused_ = false;
    }
}

void ReplayManager::UpdatePlayback(Vector3& playerPos, Vector3& cameraPos) {
    if (!isPlaying_) return;
    
    // totalFramesを超えている場合の安全チェック
    if (currentFrame_ >= currentReplay_.totalFrames) {
        if (isLoopPlay_) {
            currentFrame_ = 0; // ループ再生：最初に戻す
            playerPos = currentReplay_.playerInitPos; // 座標も初期位置に戻す
            cameraPos = currentReplay_.cameraInitPos; // カメラ座標も初期位置に戻す
            
            // ループのたびに実行用キーバッファを再生成（毎回違うブレ）
            GenerateRuntimeKeys();
        } else {
            StopPlayback(); // ループOFF：完全に停止
            return;
        }
    }

    const FrameData& currentFrame = currentReplay_.frames[currentFrame_];

    if (!isPaused_) {
        // 1. KeyboardInputへキーを注入
        BYTE keys[256] = {};
        BYTE preKeys[256] = {};

        const char* currentKeys = runtimeKeys_[currentFrame_].c_str();

        // 現在のキー状態の構築
        if (currentKeys[0] == 'L') { keys[DIK_A] = 0x80; keys[DIK_LEFT] = 0x80; }
        if (currentKeys[1] == 'R') { keys[DIK_D] = 0x80; keys[DIK_RIGHT] = 0x80; }
        if (currentKeys[2] == 'J') { keys[DIK_SPACE] = 0x80; }
        if (currentKeys[3] == 'D') { keys[DIK_LSHIFT] = 0x80; keys[DIK_RSHIFT] = 0x80; }
        if (currentKeys[4] == 'C') { keys[DIK_LCONTROL] = 0x80; keys[DIK_RCONTROL] = 0x80; }
        if (currentKeys[5] == 'W') { keys[DIK_W] = 0x80; keys[DIK_UP] = 0x80; }
        if (currentKeys[6] == 'S') { keys[DIK_S] = 0x80; keys[DIK_DOWN] = 0x80; }

        // 1フレーム前のキー状態の構築
        if (currentFrame_ > 0) {
            const char* prevKeys = runtimeKeys_[currentFrame_ - 1].c_str();
            if (prevKeys[0] == 'L') { preKeys[DIK_A] = 0x80; preKeys[DIK_LEFT] = 0x80; }
            if (prevKeys[1] == 'R') { preKeys[DIK_D] = 0x80; preKeys[DIK_RIGHT] = 0x80; }
            if (prevKeys[2] == 'J') { preKeys[DIK_SPACE] = 0x80; }
            if (prevKeys[3] == 'D') { preKeys[DIK_LSHIFT] = 0x80; preKeys[DIK_RSHIFT] = 0x80; }
            if (prevKeys[4] == 'C') { preKeys[DIK_LCONTROL] = 0x80; preKeys[DIK_RCONTROL] = 0x80; }
            if (prevKeys[5] == 'W') { preKeys[DIK_W] = 0x80; preKeys[DIK_UP] = 0x80; }
            if (prevKeys[6] == 'S') { preKeys[DIK_S] = 0x80; preKeys[DIK_DOWN] = 0x80; }
        }

        KeyboardInput::GetInstance()->SetReplayKeyStates(keys, preKeys);

        // 2. 二重発動防止：現在の物理位置と記録されている位置を比較し、
        // ズレが 0.05f 以上の一定の閾値を超えた場合のみ、正しい座標に吸着（補正）させます。
        // ※ UpdatePlayback は player_->Update() の前に呼ばれるため、現在位置は「1つ前のフレームの計算結果」です。
        Vector3 recordedPos;
        if (currentFrame_ == 0) {
            recordedPos = currentReplay_.playerInitPos;
        } else {
            recordedPos = currentReplay_.frames[currentFrame_ - 1].position;
        }

        float dx = playerPos.x - recordedPos.x;
        float dy = playerPos.y - recordedPos.y;
        float dz = playerPos.z - recordedPos.z;
        float distSq = dx * dx + dy * dy + dz * dz;

        // 位置補正(isSnapEnabled_)がOFFの時のみ、ズレ検知のエラーログを出力する
        if (distSq > 0.0025f && !isSnapEnabled_ && !forceSnapNextFrame_ && !hasLoggedDesync_) {
            std::string errorMsg = std::format(
                "リプレイのズレ検知 (フレーム: {}, ズレ量: {:.3f})\n"
                "  [現在座標] X:{:.3f}, Y:{:.3f}, Z:{:.3f}\n"
                "  [録画座標] X:{:.3f}, Y:{:.3f}, Z:{:.3f}\n"
                "  [原因] 録画時と再生時で物理演算の結果に誤差が蓄積しています。\n"
                "  [対策] インスペクターの「位置補正」をONにするか、物理演算を固定時間で行うよう変更してください。",
                currentFrame_, std::sqrt(distSq),
                playerPos.x, playerPos.y, playerPos.z,
                recordedPos.x, recordedPos.y, recordedPos.z
            );
            LogManager::GetInstance()->AddLog(LogLevel::Error, errorMsg);
            hasLoggedDesync_ = true; // スパム防止のため1再生につき1回まで
        }

        if (isSnapEnabled_ || forceSnapNextFrame_) {
            if (distSq > 0.0025f || forceSnapNextFrame_) {
                if (isInterpolationEnabled_ && currentFrame_ + 1 < static_cast<int>(currentReplay_.frames.size())) {
                    const auto& nextFrame = currentReplay_.frames[currentFrame_ + 1];
                    playerPos.x = recordedPos.x + (nextFrame.position.x - recordedPos.x) * 0.5f;
                    playerPos.y = recordedPos.y + (nextFrame.position.y - recordedPos.y) * 0.5f;
                    playerPos.z = recordedPos.z + (nextFrame.position.z - recordedPos.z) * 0.5f;
                } else {
                    playerPos = recordedPos;
                }
            }
        }
    } else {
        // ★ PAUSE中の場合は物理演算が止まらない対策として常に指定フレームの座標に強制作成・補間上書きする
        if (isInterpolationEnabled_ && currentFrame_ + 1 < static_cast<int>(currentReplay_.frames.size())) {
            const auto& nextFrame = currentReplay_.frames[currentFrame_ + 1];
            playerPos.x = currentFrame.position.x + (nextFrame.position.x - currentFrame.position.x) * 0.5f;
            playerPos.y = currentFrame.position.y + (nextFrame.position.y - currentFrame.position.y) * 0.5f;
            playerPos.z = currentFrame.position.z + (nextFrame.position.z - currentFrame.position.z) * 0.5f;
        } else {
            playerPos = currentFrame.position;
        }
    }

    // カメラ座標の同期（カメラはキー操作で物理挙動しないためダイレクトに同期・補間）
    if (isSnapEnabled_ || forceSnapNextFrame_ || isPaused_) {
        if (isInterpolationEnabled_ && currentFrame_ + 1 < static_cast<int>(currentReplay_.frames.size())) {
            const auto& nextFrame = currentReplay_.frames[currentFrame_ + 1];
            cameraPos.x = currentFrame.cameraPosition.x + (nextFrame.cameraPosition.x - currentFrame.cameraPosition.x) * 0.5f;
            cameraPos.y = currentFrame.cameraPosition.y + (nextFrame.cameraPosition.y - currentFrame.cameraPosition.y) * 0.5f;
            cameraPos.z = currentFrame.cameraPosition.z + (nextFrame.cameraPosition.z - currentFrame.cameraPosition.z) * 0.5f;
        } else {
            cameraPos = currentFrame.cameraPosition;
        }
    }

    forceSnapNextFrame_ = false;

    // フレームを進める
    if (!isPaused_) {
        currentFrame_++;
    }
}

void ReplayManager::SetCurrentFrame(int frame) {
    if (currentReplay_.totalFrames == 0) return;
    int prevFrame = currentFrame_;
    currentFrame_ = (std::max)(0, (std::min)(frame, currentReplay_.totalFrames - 1));
    if (prevFrame != currentFrame_) {
        forceSnapNextFrame_ = true;
        hasLoggedDesync_ = false;
    }
}

void ReplayManager::ApplyTimelineEdit(int frameIdx, int keyIdx, bool active) {
    ReplayTimelineEditor::ApplyTimelineEdit(currentReplay_, frameIdx, keyIdx, active);
}

void ReplayManager::SetTrackKeyRange(int trackIdx, int startFrame, int endFrame, bool active) {
    ReplayTimelineEditor::SetTrackKeyRange(currentReplay_, trackIdx, startFrame, endFrame, active);
}

void ReplayManager::ModifyBlockRange(int trackIdx, int oldStart, int oldEnd, int newStart, int newEnd) {
    ReplayTimelineEditor::ModifyBlockRange(currentReplay_, trackIdx, oldStart, oldEnd, newStart, newEnd);
}

void ReplayManager::DeleteBlockRange(int trackIdx, int startFrame, int endFrame) {
    ReplayTimelineEditor::DeleteBlockRange(currentReplay_, trackIdx, startFrame, endFrame);
}

void ReplayManager::GenerateRuntimeKeys() {
    int maxFrame = currentReplay_.totalFrames;
    if (maxFrame <= 0) return;

    runtimeKeys_.resize(maxFrame);
    
    // ベースとして元のキー状態をコピー
    for (int i = 0; i < maxFrame; ++i) {
        runtimeKeys_[i] = currentReplay_.frames[i].keys;
    }

    // Jitter設定を適用
    for (const auto& jitter : currentReplay_.jitters) {
        if (jitter.maxJitter <= 0) continue;
        
        int startF = jitter.startFrame;
        int endF = jitter.endFrame;
        if (startF < 0) startF = 0;
        if (endF >= maxFrame) endF = maxFrame - 1;
        if (startF > endF) continue;

        char keyChars[8] = "LRJDCWS";
        char targetKey = keyChars[jitter.keyIdx];

        int shiftStart = (std::rand() % (jitter.maxJitter * 2 + 1)) - jitter.maxJitter;
        int shiftEnd = (std::rand() % (jitter.maxJitter * 2 + 1)) - jitter.maxJitter;

        int newStartF = startF + shiftStart;
        int newEndF = endF + shiftEnd;

        if (newEndF < newStartF) {
            newEndF = newStartF;
        }

        // まず元の区間をクリア
        for (int i = startF; i <= endF; ++i) {
            runtimeKeys_[i][jitter.keyIdx] = '-';
        }
        
        // シフト後の区間をONにする
        for (int i = newStartF; i <= newEndF; ++i) {
            if (i >= 0 && i < maxFrame) {
                runtimeKeys_[i][jitter.keyIdx] = targetKey;
            }
        }
    }
}

void ReplayManager::RebuildMmlFromFrames(ReplayData& data) {
    ReplayTimelineEditor::RebuildMmlFromFrames(data);
}

void ReplayManager::RebuildFramesFromMml(ReplayData& data) {
    ReplayTimelineEditor::RebuildFramesFromMml(data);
}

bool ReplayManager::SaveToFile(const ReplayData& data, const std::string& filename) {
    return ReplayIO::SaveToFile(data, filename);
}

bool ReplayManager::LoadFromFile(const std::string& filepath, ReplayData& outData) {
    return ReplayIO::LoadFromFile(filepath, outData);
}

void ReplayManager::LoadSavedList() {
    savedList_ = ReplayIO::GetSavedFileList();
}

void ReplayManager::DeleteSavedFile(const std::string& filepath) {
    ReplayIO::DeleteSavedFile(filepath);
}

bool ReplayManager::CheckCollisionAt(float x, float y, MapChip2D* mapChip) const {
    if (!mapChip) return false;

    const float PLAYER_SIZE = 0.8f;
    float halfW = PLAYER_SIZE * 0.35f;
    float halfH = PLAYER_SIZE * 0.40f;
    float corners[4][2] = {
        { x - halfW, y - halfH },
        { x + halfW, y - halfH },
        { x - halfW, y + halfH },
        { x + halfW, y + halfH }
    };

    for (int i = 0; i < 4; ++i) {
        int cx = mapChip->WorldToChipX(corners[i][0]);
        int cy = mapChip->WorldToChipY(corners[i][1]);

        MapChip2D::ChipType type = mapChip->GetChipType(cx, cy);
        if (type == MapChip2D::ChipType::kBlock || type == MapChip2D::ChipType::kDeathBlock) {
            return true;
        }
    }
    return false;
}

void ReplayManager::ExecuteFastMonkeyTest(MapChip2D* mapChip, int iterations, int jitterChance) {
    ReplayTester tester;
    tester.ExecuteFastMonkeyTest(mapChip, currentReplay_.playerInitPos, currentReplay_.frames, replayHeader_.randomSeed, iterations, jitterChance, monkeyTestLogs_);
}

DifficultyScore ReplayManager::AnalyzeReplayDifficulty(const std::vector<FrameData>& replayData, MapChip2D* mapChip) {
    ReplayTester tester;
    lastAnalyzedScore_ = tester.AnalyzeReplayDifficulty(replayData, mapChip);
    return lastAnalyzedScore_;
}

std::vector<FrameData> ReplayManager::SimulateMacro(const std::vector<FrameData>& perfectMacro, MapChip2D* mapChip) {
    std::vector<FrameData> result;
    if (perfectMacro.empty() || !mapChip) return result;

    Vector3 simulatedPos = currentReplay_.playerInitPos;
    float vx = 0.0f;
    float vy = 0.0f;
    bool isGrounded = true;
    bool isDashing = false;
    float dashTimer = 0.0f;
    float dashCooldown = 0.0f;
    float dashVx = 0.0f;
    float dashVy = 0.0f;
    bool lastPressJump = false;

    // 物理パラメータ
    const float FIXED_DELTA_TIME = 1.0f / 60.0f;
    const float MOVE_SPEED = 5.0f;
    const float JUMP_POWER = 17.0f;
    const float GRAVITY = -40.0f;
    const float MAX_FALL_SPEED = -15.0f;

    for (size_t frame = 0; frame < perfectMacro.size(); ++frame) {
        FrameData currentFrameData = perfectMacro[frame];

        bool pressLeft  = (currentFrameData.keys[0] == 'L');
        bool pressRight = (currentFrameData.keys[1] == 'R');
        bool pressJump  = (currentFrameData.keys[2] == 'J');
        bool pressDash  = (currentFrameData.keys[3] == 'D');

        // ダッシュ発動判定
        if (pressDash && !isDashing && dashCooldown <= 0.0f) {
            isDashing = true;
            dashTimer = 8.0f * FIXED_DELTA_TIME;
            dashCooldown = 30.0f * FIXED_DELTA_TIME;
            float dirX = pressLeft ? -1.0f : (pressRight ? 1.0f : 1.0f);
            dashVx = dirX * MOVE_SPEED * 2.2f;
            dashVy = 0.0f;
        }

        if (isDashing) {
            vx = dashVx;
            vy = dashVy;
            dashTimer -= FIXED_DELTA_TIME;
            if (dashTimer <= 0.0f) {
                isDashing = false;
            }
        } else {
            vx = pressLeft ? -MOVE_SPEED : (pressRight ? MOVE_SPEED : 0.0f);
            if (!isGrounded) {
                vy += GRAVITY * FIXED_DELTA_TIME;
                if (vy < MAX_FALL_SPEED) vy = MAX_FALL_SPEED;
            }
        }

        if (dashCooldown > 0.0f) {
            dashCooldown -= FIXED_DELTA_TIME;
        }

        // ジャンプ発動判定
        if (pressJump && !lastPressJump && isGrounded && !isDashing) {
            vy = JUMP_POWER;
            isGrounded = false;
        }
        lastPressJump = pressJump;

        // X軸移動 & 衝突判定
        float nextX = simulatedPos.x + vx * FIXED_DELTA_TIME;
        if (CheckCollisionAt(nextX, simulatedPos.y, mapChip)) {
            vx = 0.0f;
        } else {
            simulatedPos.x = nextX;
        }

        // Y軸移動 & 衝突判定
        float nextY = simulatedPos.y + vy * FIXED_DELTA_TIME;
        if (CheckCollisionAt(simulatedPos.x, nextY, mapChip)) {
            if (vy < 0.0f) {
                isGrounded = true;
            }
            vy = 0.0f;
        } else {
            simulatedPos.y = nextY;
            if (!CheckCollisionAt(simulatedPos.x, simulatedPos.y - 0.1f, mapChip)) {
                isGrounded = false;
            }
        }

        currentFrameData.position = simulatedPos;
        result.push_back(currentFrameData);

        // 死亡判定（死亡・落下時はシミュレーション打ち切り）
        int cx = mapChip->WorldToChipX(simulatedPos.x);
        int cy = mapChip->WorldToChipY(simulatedPos.y);
        if (mapChip->GetChipType(cx, cy) == MapChip2D::ChipType::kDeathBlock || simulatedPos.y < -50.0f) {
            break; 
        }
    }

    return result;
}

void ReplayManager::ExecuteAStarAsync(const Vector3& startPos, const Vector3& goalPos, MapChip2D* mapChip, int maxNodes) {
    if (isAISearching_.load()) return; // 既に探索中ならスキップ

    if (aiSearchThread_.joinable()) {
        aiSearchThread_.join();
    }

    isAISearching_.store(true);
    aiPathStatusMsg_ = "AI探索中... (バックグラウンド計算中)";

    aiSearchThread_ = std::thread([this, startPos, goalPos, mapChip, maxNodes]() {
        PhysicsAStar astar;
        std::vector<Vector3> path;
        bool success = astar.FindValidPath(startPos, goalPos, mapChip, path, maxNodes);

        if (success) {
            aiPathPositions_ = path;
            float seconds = path.size() / 60.0f;
            aiPathStatusMsg_ = std::format("クリア可能！ (推定時間: {:.2f} 秒 / 全 {} フレーム)", seconds, path.size());
        } else {
            aiPathPositions_.clear();
            aiPathStatusMsg_ = "[詰み] ゴールまでの経路が存在しません！";
        }

        isAISearching_.store(false);
    });
}

