#pragma once
#include "Core/Utility/Structs.h"
#include <string>

struct PlayerParams {
    float moveSpeed_ = 6.0f;
    float jumpPower_ = 17.5f;
    float gravity_ = -35.0f;
    float maxFallSpeed_ = -20.0f;
    float halfWidth_ = 0.4f;
    float halfHeight_ = 0.8f;
    Vector4 colorNormal_ = { 0.2f, 0.6f, 1.0f, 1.0f };
    Vector4 colorDashed_ = { 1.0f, 1.0f, 1.0f, 1.0f };
    Vector4 colorTired_ = { 0.8f, 0.2f, 0.2f, 1.0f };
    float maxStamina_ = 110.0f;
    float deathDuration_ = 0.2f;
    float respawnDuration_ = 0.5f;
    float goalWaitTime_ = 2.0f;
    float runDustInterval_ = 0.1f;
    float chainJumpPenalty_ = 2.0f;
};

class PlayerConfig {
public:
    static void Save(const PlayerParams& params, const std::string& filepath);
    static void Load(PlayerParams& params, const std::string& filepath);
};
