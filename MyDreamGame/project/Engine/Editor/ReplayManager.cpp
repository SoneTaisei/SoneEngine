#include "ReplayManager.h"
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

void ReplayManager::LoadMacros() {
    macros_.clear();
    std::ifstream ifs("json/replay_macros.txt");
    if (!ifs.is_open()) return;

    std::string line;
    ReplayMacro* currentMacro = nullptr;

    while (std::getline(ifs, line)) {
        if (line.empty() || line[0] == '#') continue;

        if (line[0] == '[' && line[line.length() - 1] == ']') {
            macros_.push_back(ReplayMacro());
            currentMacro = &macros_.back();
            currentMacro->name = line.substr(1, line.length() - 2);
            continue;
        }

        if (currentMacro) {
            std::stringstream ss(line);
            std::string durStr, keysStr;
            if (std::getline(ss, durStr, ',') && std::getline(ss, keysStr)) {
                MacroBlock b;
                b.duration = std::stoi(durStr);
                strncpy_s(b.keys, keysStr.c_str(), sizeof(b.keys));
                currentMacro->blocks.push_back(b);
            }
        }
    }
}

void ReplayManager::SaveMacros() {
    std::filesystem::create_directories("json");
    std::ofstream ofs("json/replay_macros.txt");
    if (!ofs.is_open()) return;

    for (const auto& macro : macros_) {
        ofs << "[" << macro.name << "]" << std::endl;
        for (const auto& block : macro.blocks) {
            ofs << block.duration << "," << block.keys << std::endl;
        }
        ofs << std::endl;
    }
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
    if (currentReplay_.totalFrames == 0) return;

    int currentFrameIdx = startFrame;
    for (const auto& block : macro.blocks) {
        for (int i = 0; i < block.duration; ++i) {
            if (currentFrameIdx >= currentReplay_.totalFrames) break;

            // マクロのキー状態を適用
            // "LRJDCWS" 順
            for (int k = 0; k < 7; ++k) {
                if (block.keys[k] != '-') {
                    currentReplay_.frames[currentFrameIdx].keys[k] = block.keys[k];
                } else if (block.keys[k] == '-') {
                    // 何もしないか、Nにするか。
                    // 今回はマクロで指定されたキーに完全に上書きする（'L'などを維持しない）
                    currentReplay_.frames[currentFrameIdx].keys[k] = '-';
                }
            }
            currentFrameIdx++;
        }
    }

    // 変更をMMLに反映
    RebuildMmlFromFrames(currentReplay_);
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
    std::string filename = std::format("bug_report_{:04}{:02}{:02}_{:02}{:02}{:02}.json", 
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
        return; // 再生対象がない
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
        StopPlayback();
        return;
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

        // isSnapEnabled_ (TASモードの補正) がOFFでも、ズレ検知のログは出す
        if (distSq > 0.0025f && !forceSnapNextFrame_ && !hasLoggedDesync_) {
            std::string errorMsg = std::format(
                "リプレイのズレ検知 (フレーム: {}, ズレ量: {:.3f})\n"
                "  [現在座標] X:{:.3f}, Y:{:.3f}, Z:{:.3f}\n"
                "  [録画座標] X:{:.3f}, Y:{:.3f}, Z:{:.3f}\n"
                "  [原因] 録画時と再生時で物理演算の結果に誤差が蓄積しています。\n"
                "  [対策] TAS編集メニューの「位置補正」をONにするか、物理演算を固定時間で行うよう変更してください。",
                currentFrame_, std::sqrt(distSq),
                playerPos.x, playerPos.y, playerPos.z,
                recordedPos.x, recordedPos.y, recordedPos.z
            );
            LogManager::GetInstance()->AddLog(LogLevel::Error, errorMsg);
            hasLoggedDesync_ = true; // スパム防止のため1再生につき1回まで
        }

        if (isSnapEnabled_ || forceSnapNextFrame_) {
            if (distSq > 0.0025f || forceSnapNextFrame_) {
                playerPos = recordedPos;
            }
        }
    } else {
        // ★ PAUSE中の場合は物理演算が止まらない対策として常に指定フレームの座標に強制上書きする
        playerPos = currentFrame.position;
    }

    // カメラ座標の同期（カメラはキー操作で物理挙動しないためダイレクトに同期）
    if (isSnapEnabled_ || forceSnapNextFrame_ || isPaused_) {
        cameraPos = currentFrame.cameraPosition;
    }

    forceSnapNextFrame_ = false;

    // フレームを進める
    if (!isPaused_) {
        currentFrame_++;
    }
}

void ReplayManager::SetCurrentFrame(int frame) {
    if (!isPlaying_ || currentReplay_.totalFrames == 0) return;
    int prevFrame = currentFrame_;
    currentFrame_ = (std::max)(0, (std::min)(frame, currentReplay_.totalFrames - 1));
    if (prevFrame != currentFrame_) {
        forceSnapNextFrame_ = true;
        hasLoggedDesync_ = false;
    }
}

void ReplayManager::ApplyTimelineEdit(int frameIdx, int keyIdx, bool active) {
    if (frameIdx < 0 || frameIdx >= static_cast<int>(currentReplay_.frames.size())) return;
    
    char keyChars[8] = "LRJDCWS";
    currentReplay_.frames[frameIdx].keys[keyIdx] = active ? keyChars[keyIdx] : '-';

    // 編集時はブロックが壊れる可能性があるため、重複するJitter設定をリセットする
    auto it = std::remove_if(currentReplay_.jitters.begin(), currentReplay_.jitters.end(),
        [frameIdx, keyIdx](const JitterSetting& j) {
            return (j.keyIdx == keyIdx && frameIdx >= j.startFrame && frameIdx <= j.endFrame);
        });
    currentReplay_.jitters.erase(it, currentReplay_.jitters.end());

    // キーが変更されたため、MMLトラックを再計算する
    RebuildMmlFromFrames(currentReplay_);
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
    if (data.frames.empty()) return;

    std::vector<char> rawT0, rawT1, rawT2, rawT3, rawT4;
    rawT0.reserve(data.frames.size());
    rawT1.reserve(data.frames.size());
    rawT2.reserve(data.frames.size());
    rawT3.reserve(data.frames.size());
    rawT4.reserve(data.frames.size());

    for (const auto& frame : data.frames) {
        rawT0.push_back(frame.keys[0]); // 'L', 'R', or '-'
        rawT1.push_back(frame.keys[2]); // 'J' or '-'
        rawT2.push_back(frame.keys[3]); // 'D' or '-'
        rawT3.push_back(frame.keys[4]); // 'C' or '-'
        // 'W' or 'S' or '-'
        if (frame.keys[5] == 'W') rawT4.push_back('W');
        else if (frame.keys[6] == 'S') rawT4.push_back('S');
        else rawT4.push_back('-');
    }

    data.mmlTracks[0] = EncodeTrackToMml(rawT0);
    data.mmlTracks[1] = EncodeTrackToMml(rawT1);
    data.mmlTracks[2] = EncodeTrackToMml(rawT2);
    data.mmlTracks[3] = EncodeTrackToMml(rawT3);
    data.mmlTracks[4] = EncodeTrackToMml(rawT4);
}

void ReplayManager::RebuildFramesFromMml(ReplayData& data) {
    if (data.totalFrames == 0) return;

    auto track0 = DecodeMmlToTrack(data.mmlTracks[0], data.totalFrames);
    auto track1 = DecodeMmlToTrack(data.mmlTracks[1], data.totalFrames);
    auto track2 = DecodeMmlToTrack(data.mmlTracks[2], data.totalFrames);
    auto track3 = DecodeMmlToTrack(data.mmlTracks[3], data.totalFrames);
    auto track4 = DecodeMmlToTrack(data.mmlTracks[4], data.totalFrames);

    data.frames.resize(data.totalFrames);
    for (int i = 0; i < data.totalFrames; ++i) {
        data.frames[i].keys[0] = (i < static_cast<int>(track0.size())) ? track0[i] : '-';
        data.frames[i].keys[1] = (data.frames[i].keys[0] == 'L' || data.frames[i].keys[0] == 'R') ? data.frames[i].keys[0] : '-'; // 重複除去用
        data.frames[i].keys[2] = (i < static_cast<int>(track1.size())) ? track1[i] : '-';
        data.frames[i].keys[3] = (i < static_cast<int>(track2.size())) ? track2[i] : '-';
        data.frames[i].keys[4] = (i < static_cast<int>(track3.size())) ? track3[i] : '-';
        
        char t4Val = (i < static_cast<int>(track4.size())) ? track4[i] : '-';
        data.frames[i].keys[5] = (t4Val == 'W') ? 'W' : '-';
        data.frames[i].keys[6] = (t4Val == 'S') ? 'S' : '-';
        data.frames[i].keys[7] = '\0';
        
        // 座標情報はパース済みの frames から引き継ぐか、初期化
        if (data.frames[i].position.x == 0 && data.frames[i].position.y == 0 && data.frames[i].position.z == 0) {
            data.frames[i].position = data.playerInitPos;
        }
        if (data.frames[i].cameraPosition.x == 0 && data.frames[i].cameraPosition.y == 0 && data.frames[i].cameraPosition.z == 0) {
            data.frames[i].cameraPosition = data.cameraInitPos;
        }
    }
}

std::string ReplayManager::EncodeTrackToMml(const std::vector<char>& rawTrack) {
    if (rawTrack.empty()) return "";
    std::stringstream ss;
    char lastChar = rawTrack[0];
    int count = 1;

    for (size_t i = 1; i < rawTrack.size(); ++i) {
        char currentChar = rawTrack[i];
        if (currentChar == lastChar) {
            count++;
        } else {
            // '-' は無入力 'N' として表現
            char code = (lastChar == '-') ? 'N' : lastChar;
            ss << code << count << " ";
            lastChar = currentChar;
            count = 1;
        }
    }
    char code = (lastChar == '-') ? 'N' : lastChar;
    ss << code << count;
    return ss.str();
}

std::vector<char> ReplayManager::DecodeMmlToTrack(const std::string& mmlStr, int expectedFrames) {
    std::vector<char> rawTrack;
    rawTrack.reserve(expectedFrames);
    std::stringstream ss(mmlStr);
    std::string token;

    while (ss >> token) {
        if (token.empty()) continue;
        char code = token[0];
        // 'N' は無入力 '-'
        char val = (code == 'N') ? '-' : code;
        int duration = 0;
        if (token.length() > 1) {
            duration = std::stoi(token.substr(1));
        } else {
            duration = 1;
        }
        for (int i = 0; i < duration; ++i) {
            rawTrack.push_back(val);
        }
    }
    
    // 長さ調整
    if (static_cast<int>(rawTrack.size()) < expectedFrames) {
        rawTrack.insert(rawTrack.end(), expectedFrames - rawTrack.size(), '-');
    }
    return rawTrack;
}

bool ReplayManager::SaveToFile(const ReplayData& data, const std::string& filename) {
    std::filesystem::create_directories("resources/json/saved_replays");
    std::string filepath = "resources/json/saved_replays/" + filename;
    
    // 拡張子の補正
    if (filepath.find(".mml") == std::string::npos) {
        filepath += ".mml";
    }

    std::ofstream ofs(filepath);
    if (!ofs.is_open()) return false;

    // メタデータ
    ofs << "# MML Replay File" << std::endl;
    ofs << "[Metadata]" << std::endl;
    ofs << "Date=" << data.dateStr << std::endl;
    ofs << "StageFilename=" << data.stageFilename << std::endl;
    ofs << "TotalFrames=" << data.totalFrames << std::endl;
    ofs << "PlayerInitPos=" << data.playerInitPos.x << "," << data.playerInitPos.y << "," << data.playerInitPos.z << std::endl;
    ofs << "CameraInitPos=" << data.cameraInitPos.x << "," << data.cameraInitPos.y << "," << data.cameraInitPos.z << std::endl;
    ofs << std::endl;

    // 生マップデータセクション
    if (!data.mapDataStr.empty()) {
        ofs << "[MapData]" << std::endl;
        ofs << data.mapDataStr;
        if (data.mapDataStr.back() != '\n') ofs << std::endl;
        ofs << std::endl;
    }

    // MML トラックセクション
    ofs << "[MML]" << std::endl;
    ofs << "T0_LeftRight=" << data.mmlTracks[0] << std::endl;
    ofs << "T1_Jump=" << data.mmlTracks[1] << std::endl;
    ofs << "T2_Dash=" << data.mmlTracks[2] << std::endl;
    ofs << "T3_Cling=" << data.mmlTracks[3] << std::endl;
    ofs << "T4_UpDown=" << data.mmlTracks[4] << std::endl;
    ofs << std::endl;

    // 動的ブレ設定 (Jitters)
    if (!data.jitters.empty()) {
        ofs << "[Jitters]" << std::endl;
        for (const auto& j : data.jitters) {
            ofs << j.keyIdx << "," << j.startFrame << "," << j.endFrame << "," << j.maxJitter << std::endl;
        }
        ofs << std::endl;
    }

    // 1フレームずつの状態データ (STR)
    // フォーマット: F0000|PlayerX,Y,Z|CamX,CamY,CamZ|LRJDC|R,G,B,A|ScaleX,Y,Z|RotX,Y,Z
    ofs << "[STR]" << std::endl;
    for (int i = 0; i < data.totalFrames; ++i) {
        const auto& frame = data.frames[i];
        ofs << "F" << std::setw(4) << std::setfill('0') << i << "|"
            << frame.position.x << "," << frame.position.y << "," << frame.position.z << "|"
            << frame.cameraPosition.x << "," << frame.cameraPosition.y << "," << frame.cameraPosition.z << "|"
            << frame.keys << "|"
            << frame.color.x << "," << frame.color.y << "," << frame.color.z << "," << frame.color.w << "|"
            << frame.scale.x << "," << frame.scale.y << "," << frame.scale.z << "|"
            << frame.rotation.x << "," << frame.rotation.y << "," << frame.rotation.z << std::endl;
    }

    ofs.close();
    LoadSavedList(); // リストの更新
    return true;
}

bool ReplayManager::LoadFromFile(const std::string& filepath, ReplayData& outData) {
    std::ifstream ifs(filepath);
    if (!ifs.is_open()) return false;

    outData.frames.clear();
    outData.filename = std::filesystem::path(filepath).filename().string();
    
    std::string line;
    std::string currentSection = "";

    while (std::getline(ifs, line)) {
        // 空行やコメントスキップ
        if (line.empty() || line[0] == '#') continue;

        // セクションの切り替え
        if (line[0] == '[' && line[line.length() - 1] == ']') {
            currentSection = line.substr(1, line.length() - 2);
            continue;
        }

        std::stringstream ss(line);
        std::string key, value;

        if (currentSection == "MapData") {
            outData.mapDataStr += line + "\n";
        } else if (currentSection == "Metadata") {
            if (std::getline(ss, key, '=') && std::getline(ss, value)) {
                if (key == "Date") {
                    outData.dateStr = value;
                } else if (key == "StageFilename") {
                    outData.stageFilename = value;
                } else if (key == "TotalFrames") {
                    outData.totalFrames = std::stoi(value);
                } else if (key == "PlayerInitPos") {
                    std::stringstream vss(value);
                    std::string x, y, z;
                    if (std::getline(vss, x, ',') && std::getline(vss, y, ',') && std::getline(vss, z)) {
                        outData.playerInitPos = { std::stof(x), std::stof(y), std::stof(z) };
                    }
                } else if (key == "CameraInitPos") {
                    std::stringstream vss(value);
                    std::string x, y, z;
                    if (std::getline(vss, x, ',') && std::getline(vss, y, ',') && std::getline(vss, z)) {
                        outData.cameraInitPos = { std::stof(x), std::stof(y), std::stof(z) };
                    }
                }
            }
        } else if (currentSection == "MML") {
            if (std::getline(ss, key, '=') && std::getline(ss, value)) {
                if (key == "T0_LeftRight") outData.mmlTracks[0] = value;
                else if (key == "T1_Jump") outData.mmlTracks[1] = value;
                else if (key == "T2_Dash") outData.mmlTracks[2] = value;
                else if (key == "T3_Cling") outData.mmlTracks[3] = value;
                else if (key == "T4_UpDown") outData.mmlTracks[4] = value;
            }
        } else if (currentSection == "Jitters") {
            std::stringstream jss(line);
            std::string p1, p2, p3, p4;
            if (std::getline(jss, p1, ',') && std::getline(jss, p2, ',') && std::getline(jss, p3, ',') && std::getline(jss, p4)) {
                JitterSetting j;
                j.keyIdx = std::stoi(p1);
                j.startFrame = std::stoi(p2);
                j.endFrame = std::stoi(p3);
                j.maxJitter = std::stoi(p4);
                outData.jitters.push_back(j);
            }
        } else if (currentSection == "STR") {
            // 新フォーマット: F0000|PlayerX,Y,Z|CamX,CamY,CamZ|LRJDC|R,G,B,A|ScaleX,Y,Z|RotX,Y,Z
            // 旧フォーマット: F0000|PlayerX,Y,Z|CamX,CamY,CamZ|LRJDC|R,G,B,A  または F0000|PlayerX,Y,Z|LRJDC
            std::string parts[7];
            int partCount = 0;
            std::string part;
            while (std::getline(ss, part, '|') && partCount < 7) {
                parts[partCount++] = part;
            }
            
            if (partCount >= 3) {
                FrameData frame;
                std::string posStr = parts[1];
                std::string keysStr = parts[2];
                std::string camPosStr = "";
                std::string colorStr = "";
                std::string scaleStr = "";
                std::string rotStr = "";
                
                if (partCount == 4) {
                    camPosStr = parts[2];
                    keysStr = parts[3];
                } else if (partCount >= 5) {
                    camPosStr = parts[2];
                    keysStr = parts[3];
                    colorStr = parts[4];
                    if (partCount >= 7) {
                        scaleStr = parts[5];
                        rotStr = parts[6];
                    }
                }
                
                // 座標パース
                std::stringstream pss(posStr);
                std::string x, y, z;
                if (std::getline(pss, x, ',') && std::getline(pss, y, ',') && std::getline(pss, z)) {
                    frame.position = { std::stof(x), std::stof(y), std::stof(z) };
                }
                
                // カメラ座標パース
                if (!camPosStr.empty()) {
                    std::stringstream cpss(camPosStr);
                    if (std::getline(cpss, x, ',') && std::getline(cpss, y, ',') && std::getline(cpss, z)) {
                        frame.cameraPosition = { std::stof(x), std::stof(y), std::stof(z) };
                    }
                } else {
                    frame.cameraPosition = outData.cameraInitPos;
                }

                // キー状態コピー
                for (int i = 0; i < 7 && i < static_cast<int>(keysStr.length()); ++i) {
                    frame.keys[i] = keysStr[i];
                }
                // もし元のファイルが古くて文字数が足りなかった場合、残りを '-' で埋める
                for (int i = static_cast<int>(keysStr.length()); i < 7; ++i) {
                    frame.keys[i] = '-';
                }
                frame.keys[7] = '\0';
                
                // カラーのパース
                if (!colorStr.empty()) {
                    std::stringstream css(colorStr);
                    std::string r, g, b, a;
                    if (std::getline(css, r, ',') && std::getline(css, g, ',') && std::getline(css, b, ',') && std::getline(css, a)) {
                        frame.color = { std::stof(r), std::stof(g), std::stof(b), std::stof(a) };
                    }
                }

                // スケールのパース
                if (!scaleStr.empty()) {
                    std::stringstream sss(scaleStr);
                    std::string sx, sy, sz;
                    if (std::getline(sss, sx, ',') && std::getline(sss, sy, ',') && std::getline(sss, sz)) {
                        frame.scale = { std::stof(sx), std::stof(sy), std::stof(sz) };
                    }
                }

                // 回転のパース
                if (!rotStr.empty()) {
                    std::stringstream rss(rotStr);
                    std::string rx, ry, rz;
                    if (std::getline(rss, rx, ',') && std::getline(rss, ry, ',') && std::getline(rss, rz)) {
                        frame.rotation = { std::stof(rx), std::stof(ry), std::stof(rz) };
                    }
                }
                
                outData.frames.push_back(frame);
            }
        }
    }

    ifs.close();

    // 整合性を取るため、MMLから再構築するか、またはパース済みの frames を信用する
    // STR から frames が正しく読めていれば MML も一致しているはず
    if (outData.frames.empty() && outData.totalFrames > 0) {
        RebuildFramesFromMml(outData);
    } else {
        outData.totalFrames = static_cast<int>(outData.frames.size());
    }

    return true;
}

void ReplayManager::LoadSavedList() {
    savedList_.clear();
    std::filesystem::create_directories("resources/json/saved_replays");
    for (const auto& entry : std::filesystem::directory_iterator("resources/json/saved_replays")) {
        if (entry.is_regular_file() && entry.path().extension() == ".mml") {
            savedList_.push_back(entry.path().filename().string());
        }
    }
}

void ReplayManager::DeleteSavedFile(const std::string& filepath) {
    std::string fullpath = "resources/json/saved_replays/" + filepath;
    if (std::filesystem::exists(fullpath)) {
        std::filesystem::remove(fullpath);
        LoadSavedList();
    }
}
