#include "ReplayTester.h"
#include "Game2D/MapChip2D.h"
#include "ReplayManager.h"
#include "ReplayIO.h"
#include "Core/Utility/LogManager.h"
#include <ctime>
#include <cmath>
#include <iomanip>
#include <format>

void ReplayTester::ExecuteFastMonkeyTest(
    MapChip2D* mapChip, 
    const Vector3& playerInitPos, 
    const std::vector<FrameData>& originalFrames, 
    uint32_t randomSeed,
    int iterations, 
    int jitterChance, 
    std::vector<std::string>& outLogs) 
{
    outLogs.clear();
    
    if (originalFrames.empty()) {
        std::string warnMsg = "モンキーテストを開始できません: リプレイデータが空です。";
        outLogs.push_back("[WARN] " + warnMsg);
        LogManager::GetInstance()->AddLog(LogLevel::Warning, warnMsg);
        return;
    }
    if (!mapChip) {
        std::string warnMsg = "モンキーテストを開始できません: マップデータが無効です。";
        outLogs.push_back("[WARN] " + warnMsg);
        LogManager::GetInstance()->AddLog(LogLevel::Warning, warnMsg);
        return;
    }

    char timeBuffer[64];
    std::time_t t = std::time(nullptr);
    std::tm tm;
    localtime_s(&tm, &t);
    std::strftime(timeBuffer, sizeof(timeBuffer), "%Y/%m/%d %H:%M:%S", &tm);

    std::string startMsg = "モンキーテスト開始 - 試行回数: " + std::to_string(iterations) + 
                           ", Jitter発生率: " + std::to_string(jitterChance) + "% - " + timeBuffer;
    outLogs.push_back("[INFO] " + startMsg);
    LogManager::GetInstance()->AddLog(LogLevel::Info, startMsg);

    int totalDetectedBugs = 0;
    DeterministicRandom testRandom(randomSeed);

    for (int iter = 0; iter < iterations; ++iter) {
        uint32_t currentSeed = randomSeed + iter;
        testRandom.Initialize(currentSeed);

        bool bugDetectedInIter = false;
        int bugFrame = -1;
        std::string bugReason = "";

        Vector3 simulatedPos = playerInitPos;
        float vx = 0.0f;
        float vy = 0.0f;
        bool isGrounded = true;
        bool isDashing = false;
        float dashTimer = 0.0f;
        float dashCooldown = 0.0f;
        float dashVx = 0.0f;
        float dashVy = 0.0f;
        bool lastPressJump = false;

        Vector3 previousPos = simulatedPos;
        int zeroMoveStreak = 0;

        std::vector<FrameData> testReplayFrames = originalFrames;

        for (int frame = 0; frame < static_cast<int>(originalFrames.size()); ++frame) {
            FrameData& currentFrameData = testReplayFrames[frame];

            bool pressLeft  = (currentFrameData.keys[0] == 'L');
            bool pressRight = (currentFrameData.keys[1] == 'R');
            bool pressJump  = (currentFrameData.keys[2] == 'J');
            bool pressDash  = (currentFrameData.keys[3] == 'D');

            if (testRandom.GetRange(0, 100) < jitterChance) {
                pressJump = !pressJump;
                currentFrameData.keys[2] = pressJump ? 'J' : '-';
            }

            const float FIXED_DELTA_TIME = 1.0f / 60.0f;
            const float MOVE_SPEED = 5.0f;
            const float JUMP_POWER = 17.0f;
            const float GRAVITY = -40.0f;
            const float MAX_FALL_SPEED = -15.0f;

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

            if (pressJump && !lastPressJump && isGrounded && !isDashing) {
                vy = JUMP_POWER;
                isGrounded = false;
            }
            lastPressJump = pressJump;

            float nextX = simulatedPos.x + vx * FIXED_DELTA_TIME;
            if (CheckCollisionAt(nextX, simulatedPos.y, mapChip)) {
                vx = 0.0f;
            } else {
                simulatedPos.x = nextX;
            }

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

            float dx = simulatedPos.x - previousPos.x;
            float dy = simulatedPos.y - previousPos.y;
            float distSq = dx * dx + dy * dy;

            if (distSq < 0.0001f) {
                zeroMoveStreak++;
            } else {
                zeroMoveStreak = 0;
            }

            if (simulatedPos.y < -100.0f || simulatedPos.y > 5000.0f || std::isnan(simulatedPos.x) || std::isnan(simulatedPos.y)) {
                bugDetectedInIter = true;
                bugFrame = frame;
                bugReason = "落下または座標破綻を検知 (Pos: " + std::to_string((int)simulatedPos.x) + ", " + std::to_string((int)simulatedPos.y) + ")";
                break;
            }
            else if (zeroMoveStreak > 180) {
                bugDetectedInIter = true;
                bugFrame = frame;
                bugReason = "3秒間以上の位置スタック（進行阻害）を検知";
                break;
            }

            previousPos = simulatedPos;
        }

        if (bugDetectedInIter) {
            totalDetectedBugs++;
            std::string logMsg = "テストケース #" + std::to_string(iter + 1) + 
                                 " (Seed: " + std::to_string(currentSeed) + ") Frame " + 
                                 std::to_string(bugFrame) + " にて異常検出: " + bugReason;
            outLogs.push_back("[BUG DETECTED] " + logMsg);
            LogManager::GetInstance()->AddLog(LogLevel::Error, "モンキーテスト異常検出: " + logMsg);

            ReplayData bugReplay = ReplayManager::GetInstance()->GetCurrentReplay();
            bugReplay.filename = "bug_report_monkey_" + std::to_string(iter + 1) + ".json";
            bugReplay.frames = testReplayFrames;
            bugReplay.totalFrames = bugFrame + 1;
            if (bugReplay.frames.size() > static_cast<size_t>(bugReplay.totalFrames)) {
                bugReplay.frames.resize(bugReplay.totalFrames);
            }
            ReplayIO::SaveToFile(bugReplay, bugReplay.filename);
        }
    }

    if (totalDetectedBugs == 0) {
        std::string successMsg = "モンキーテスト完了: 全 " + std::to_string(iterations) + " 回の試行でバグは検知されませんでした。";
        outLogs.push_back("[SUCCESS] " + successMsg);
        LogManager::GetInstance()->AddLog(LogLevel::Info, successMsg);
    } else {
        std::string summaryMsg = "モンキーテスト完了: 合計 " + std::to_string(totalDetectedBugs) + " 件のバグ検知ログを出力しました。";
        outLogs.push_back("[SUMMARY] " + summaryMsg);
        LogManager::GetInstance()->AddLog(LogLevel::Warning, summaryMsg);
    }
}

bool ReplayTester::CheckCollisionAt(float x, float y, MapChip2D* mapChip) const {
    if (!mapChip) return false;

    const float PLAYER_SIZE = 0.8f;
    float halfW = PLAYER_SIZE * 0.35f;
    float halfH = PLAYER_SIZE * 0.40f;

    float left = x - halfW;
    float right = x + halfW;
    float top = y + halfH;
    float bottom = y - halfH;

    int minX = mapChip->WorldToChipX(left);
    int maxX = mapChip->WorldToChipX(right);
    int minY = mapChip->WorldToChipY(bottom);
    int maxY = mapChip->WorldToChipY(top);

    for (int cx = minX; cx <= maxX; ++cx) {
        for (int cy = minY; cy <= maxY; ++cy) {
            MapChip2D::ChipType type = mapChip->GetChipType(cx, cy);
            if (type == MapChip2D::ChipType::kBlock) {
                return true;
            }
        }
    }
    return false;
}

float ReplayTester::CalculateDistanceToGround(const Vector3& playerPos, MapChip2D* mapChip) const {
    if (!mapChip) return 999.0f;

    const float PLAYER_SIZE = 0.8f;
    float halfW = PLAYER_SIZE * 0.35f;
    float halfH = PLAYER_SIZE * 0.40f;
    float footY = playerPos.y - halfH;

    float checkStep = 0.05f;
    float maxCheckDist = 4.0f;

    for (float dist = 0.0f; dist <= maxCheckDist; dist += checkStep) {
        float testY = footY - dist;
        float testXs[3] = { playerPos.x - halfW, playerPos.x, playerPos.x + halfW };
        
        for (int j = 0; j < 3; ++j) {
            int cx = mapChip->WorldToChipX(testXs[j]);
            int cy = mapChip->WorldToChipY(testY);

            MapChip2D::ChipType type = mapChip->GetChipType(cx, cy);
            if (type == MapChip2D::ChipType::kBlock || 
                type == MapChip2D::ChipType::kDeathBlock || 
                type == MapChip2D::ChipType::kOneWayBlock) {
                
                float blockTopY = mapChip->ChipToWorldY(cy) + mapChip->GetChipSize();
                float actualDist = footY - blockTopY;
                if (actualDist < 0.0f) actualDist = 0.0f;
                return actualDist;
            }
        }
    }
    return maxCheckDist;
}

DifficultyScore ReplayTester::AnalyzeReplayDifficulty(const std::vector<FrameData>& replayData, MapChip2D* mapChip) {
    DifficultyScore score;
    if (replayData.empty()) {
        return score;
    }

    int inputChanges = 0;
    int zeroSpeedFrames = 0;
    float tightJumpSum = 0.0f;
    int jumpCount = 0;

    for (size_t i = 1; i < replayData.size(); ++i) {
        bool keyChanged = false;
        for (int k = 0; k < 7; ++k) {
            if (replayData[i].keys[k] != replayData[i - 1].keys[k]) {
                keyChanged = true;
                break;
            }
        }
        if (keyChanged) {
            inputChanges++;
        }

        float vx = replayData[i].position.x - replayData[i - 1].position.x;
        float vy = replayData[i].position.y - replayData[i - 1].position.y;
        if (std::abs(vx) < 0.01f && std::abs(vy) < 0.01f) {
            zeroSpeedFrames++;
        }

        bool prevJump = (replayData[i - 1].keys[2] == 'J');
        bool currJump = (replayData[i].keys[2] == 'J');
        if (currJump && !prevJump) {
            jumpCount++;
            if (mapChip) {
                float distanceToGround = CalculateDistanceToGround(replayData[i].position, mapChip);
                if (distanceToGround < 0.5f) {
                    tightJumpSum += (0.5f - distanceToGround) * 200.0f;
                }
            } else {
                float distToEdgeSq = std::abs(vx);
                if (distToEdgeSq < 0.05f) {
                    tightJumpSum += 80.0f;
                } else {
                    tightJumpSum += 30.0f;
                }
            }
        }
    }

    float totalSeconds = replayData.size() / 60.0f;
    if (totalSeconds > 0.0f) {
        score.averageAPM = (inputChanges / totalSeconds) * 60.0f;
        score.stagnationDuration = zeroSpeedFrames / 60.0f;
    }
    score.maxPrecisionScore = jumpCount > 0 ? (tightJumpSum / jumpCount) : 0.0f;

    score.finalCalculatedDifficulty = (score.averageAPM * 0.4f) + (score.maxPrecisionScore * 0.5f) - (score.stagnationDuration * 0.1f);
    if (score.finalCalculatedDifficulty < 0.0f) score.finalCalculatedDifficulty = 0.0f;

    return score;
}
