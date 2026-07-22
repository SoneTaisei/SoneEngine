#pragma once
#include "Core/Utility/Structs.h"
#include <vector>
#include <string>

class MapChip2D;
struct FrameData;
struct DifficultyScore;

class ReplayTester {
public:
    ReplayTester() = default;
    ~ReplayTester() = default;

    void ExecuteFastMonkeyTest(MapChip2D* mapChip, const Vector3& playerInitPos, const std::vector<FrameData>& originalFrames, uint32_t randomSeed, int iterations, int jitterChance, std::vector<std::string>& outLogs);
    DifficultyScore AnalyzeReplayDifficulty(const std::vector<FrameData>& replayData, MapChip2D* mapChip);

private:
    bool CheckCollisionAt(float x, float y, MapChip2D* mapChip) const;
    float CalculateDistanceToGround(const Vector3& playerPos, MapChip2D* mapChip) const;
};
