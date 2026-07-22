#include "ReplayTimelineEditor.h"
#include "ReplayManager.h"
#include <algorithm>
#include <sstream>

static void GetTrackKeyInfo(int trackIdx, int& outKeyPos, char& outKeyChar) {
    switch (trackIdx) {
    case 0: outKeyPos = 0; outKeyChar = 'L'; break;
    case 1: outKeyPos = 1; outKeyChar = 'R'; break;
    case 2: outKeyPos = 5; outKeyChar = 'W'; break;
    case 3: outKeyPos = 6; outKeyChar = 'S'; break;
    case 4: outKeyPos = 2; outKeyChar = 'J'; break;
    case 5: outKeyPos = 3; outKeyChar = 'D'; break;
    case 6: outKeyPos = 4; outKeyChar = 'C'; break;
    default: outKeyPos = 0; outKeyChar = '-'; break;
    }
}

void ReplayTimelineEditor::ApplyTimelineEdit(ReplayData& data, int frameIdx, int keyIdx, bool active) {
    if (frameIdx < 0 || frameIdx >= static_cast<int>(data.frames.size())) return;
    
    char keyChars[8] = "LRJDCWS";
    data.frames[frameIdx].keys[keyIdx] = active ? keyChars[keyIdx] : '-';

    // 編集時はブロックが壊れる可能性があるため、重複するJitter設定をリセットする
    auto it = std::remove_if(data.jitters.begin(), data.jitters.end(),
        [frameIdx, keyIdx](const JitterSetting& j) {
            return (j.keyIdx == keyIdx && frameIdx >= j.startFrame && frameIdx <= j.endFrame);
        });
    data.jitters.erase(it, data.jitters.end());

    // キーが変更されたため、MMLトラックを再計算する
    RebuildMmlFromFrames(data);
}

void ReplayTimelineEditor::SetTrackKeyRange(ReplayData& data, int trackIdx, int startFrame, int endFrame, bool active) {
    if (trackIdx < 0 || trackIdx >= 7) return;
    if (startFrame > endFrame) std::swap(startFrame, endFrame);
    if (startFrame < 0) startFrame = 0;

    int keyPos;
    char keyChar;
    GetTrackKeyInfo(trackIdx, keyPos, keyChar);

    if (endFrame > static_cast<int>(data.frames.size())) {
        FrameData fillFrame = data.frames.empty() ? FrameData{} : data.frames.back();
        for (int k = 0; k < 7; ++k) fillFrame.keys[k] = '-';
        fillFrame.keys[7] = '\0';
        data.frames.resize(endFrame, fillFrame);
    }

    for (int f = startFrame; f < endFrame && f < static_cast<int>(data.frames.size()); ++f) {
        data.frames[f].keys[keyPos] = active ? keyChar : '-';
    }

    data.totalFrames = static_cast<int>(data.frames.size());
    RebuildMmlFromFrames(data);
}

void ReplayTimelineEditor::ModifyBlockRange(ReplayData& data, int trackIdx, int oldStart, int oldEnd, int newStart, int newEnd) {
    if (trackIdx < 0 || trackIdx >= 7) return;
    int keyPos;
    char keyChar;
    GetTrackKeyInfo(trackIdx, keyPos, keyChar);

    // 1. 旧範囲の該当キーをクリア
    int validOldStart = (std::max)(0, oldStart);
    int validOldEnd = (std::min)(static_cast<int>(data.frames.size()), oldEnd);
    for (int f = validOldStart; f < validOldEnd; ++f) {
        data.frames[f].keys[keyPos] = '-';
    }

    // 2. 新範囲に必要なフレームを確保
    if (newEnd > static_cast<int>(data.frames.size())) {
        FrameData fillFrame = data.frames.empty() ? FrameData{} : data.frames.back();
        for (int k = 0; k < 7; ++k) fillFrame.keys[k] = '-';
        fillFrame.keys[7] = '\0';
        data.frames.resize(newEnd, fillFrame);
    }

    // 3. 新範囲にキーをセット
    int validNewStart = (std::max)(0, newStart);
    for (int f = validNewStart; f < newEnd && f < static_cast<int>(data.frames.size()); ++f) {
        data.frames[f].keys[keyPos] = keyChar;
    }

    data.totalFrames = static_cast<int>(data.frames.size());
    RebuildMmlFromFrames(data);
}

void ReplayTimelineEditor::DeleteBlockRange(ReplayData& data, int trackIdx, int startFrame, int endFrame) {
    SetTrackKeyRange(data, trackIdx, startFrame, endFrame, false);
}

void ReplayTimelineEditor::ApplyMacro(ReplayData& data, int startFrame, const ReplayMacro& macro) {
    if (data.totalFrames == 0) return;

    int currentFrameIdx = startFrame;
    for (const auto& block : macro.blocks) {
        for (int i = 0; i < block.duration; ++i) {
            if (currentFrameIdx >= data.totalFrames) {
                data.frames.push_back(data.frames.back());
                data.totalFrames++;
            }

            for (int k = 0; k < 7; ++k) {
                if (block.keys[k] != '-') {
                    data.frames[currentFrameIdx].keys[k] = block.keys[k];
                } else if (block.keys[k] == '-') {
                    data.frames[currentFrameIdx].keys[k] = '-';
                }
            }
            currentFrameIdx++;
        }
    }

    RebuildMmlFromFrames(data);
    
    AppliedMacro am;
    am.name = macro.name;
    am.startFrame = startFrame;
    am.duration = currentFrameIdx - startFrame;
    data.appliedMacros.push_back(am);
}

void ReplayTimelineEditor::RebuildMmlFromFrames(ReplayData& data) {
    if (data.frames.empty()) return;

    std::vector<char> rawT0, rawT1, rawT2, rawT3, rawT4;
    rawT0.reserve(data.frames.size());
    rawT1.reserve(data.frames.size());
    rawT2.reserve(data.frames.size());
    rawT3.reserve(data.frames.size());
    rawT4.reserve(data.frames.size());

    for (const auto& frame : data.frames) {
        rawT0.push_back(frame.keys[0]);
        rawT1.push_back(frame.keys[2]);
        rawT2.push_back(frame.keys[3]);
        rawT3.push_back(frame.keys[4]);
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

void ReplayTimelineEditor::RebuildFramesFromMml(ReplayData& data) {
    if (data.totalFrames == 0) return;

    auto track0 = DecodeMmlToTrack(data.mmlTracks[0], data.totalFrames);
    auto track1 = DecodeMmlToTrack(data.mmlTracks[1], data.totalFrames);
    auto track2 = DecodeMmlToTrack(data.mmlTracks[2], data.totalFrames);
    auto track3 = DecodeMmlToTrack(data.mmlTracks[3], data.totalFrames);
    auto track4 = DecodeMmlToTrack(data.mmlTracks[4], data.totalFrames);

    data.frames.resize(data.totalFrames);
    for (int i = 0; i < data.totalFrames; ++i) {
        data.frames[i].keys[0] = (i < static_cast<int>(track0.size())) ? track0[i] : '-';
        data.frames[i].keys[1] = (data.frames[i].keys[0] == 'L' || data.frames[i].keys[0] == 'R') ? data.frames[i].keys[0] : '-';
        data.frames[i].keys[2] = (i < static_cast<int>(track1.size())) ? track1[i] : '-';
        data.frames[i].keys[3] = (i < static_cast<int>(track2.size())) ? track2[i] : '-';
        data.frames[i].keys[4] = (i < static_cast<int>(track3.size())) ? track3[i] : '-';
        
        char t4Val = (i < static_cast<int>(track4.size())) ? track4[i] : '-';
        data.frames[i].keys[5] = (t4Val == 'W') ? 'W' : '-';
        data.frames[i].keys[6] = (t4Val == 'S') ? 'S' : '-';
        data.frames[i].keys[7] = '\0';
        
        if (data.frames[i].position.x == 0 && data.frames[i].position.y == 0 && data.frames[i].position.z == 0) {
            data.frames[i].position = data.playerInitPos;
        }
        if (data.frames[i].cameraPosition.x == 0 && data.frames[i].cameraPosition.y == 0 && data.frames[i].cameraPosition.z == 0) {
            data.frames[i].cameraPosition = data.cameraInitPos;
        }
    }
}

std::string ReplayTimelineEditor::EncodeTrackToMml(const std::vector<char>& rawTrack) {
    if (rawTrack.empty()) return "";
    std::stringstream ss;
    char lastChar = rawTrack[0];
    int count = 1;

    for (size_t i = 1; i < rawTrack.size(); ++i) {
        char currentChar = rawTrack[i];
        if (currentChar == lastChar) {
            count++;
        } else {
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

std::vector<char> ReplayTimelineEditor::DecodeMmlToTrack(const std::string& mmlStr, int expectedFrames) {
    std::vector<char> rawTrack;
    rawTrack.reserve(expectedFrames);
    std::stringstream ss(mmlStr);
    std::string token;

    while (ss >> token) {
        if (token.empty()) continue;
        char code = token[0];
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
    
    if (static_cast<int>(rawTrack.size()) < expectedFrames) {
        rawTrack.insert(rawTrack.end(), expectedFrames - rawTrack.size(), '-');
    }
    return rawTrack;
}
