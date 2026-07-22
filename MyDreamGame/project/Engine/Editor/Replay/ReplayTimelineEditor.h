#pragma once
#include <string>
#include <vector>

struct ReplayData;
struct ReplayMacro;

class ReplayTimelineEditor {
public:
    static void ApplyTimelineEdit(ReplayData& data, int frameIdx, int keyIdx, bool active);
    static void SetTrackKeyRange(ReplayData& data, int trackIdx, int startFrame, int endFrame, bool active);
    static void ModifyBlockRange(ReplayData& data, int trackIdx, int oldStart, int oldEnd, int newStart, int newEnd);
    static void DeleteBlockRange(ReplayData& data, int trackIdx, int startFrame, int endFrame);
    static void ApplyMacro(ReplayData& data, int startFrame, const ReplayMacro& macro);
    static void RebuildMmlFromFrames(ReplayData& data);
    static void RebuildFramesFromMml(ReplayData& data);

private:
    static std::string EncodeTrackToMml(const std::vector<char>& rawTrack);
    static std::vector<char> DecodeMmlToTrack(const std::string& mmlStr, int expectedFrames);
};
