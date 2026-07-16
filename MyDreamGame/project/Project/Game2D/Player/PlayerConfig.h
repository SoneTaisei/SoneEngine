#pragma once
#include "Core/Utility/Structs.h"
#include <string>

struct PlayerParams {
    float moveSpeed_ = 5.0f;
    float jumpPower_ = 17.0f;
    float gravity_ = -40.0f;
    float maxFallSpeed_ = -15.0f;
    float dashDuration_ = 0.15f;
    float dashSpeed_ = 15.0f;
    float dashEndUpwardVelocity_ = 10.0f;
    Vector4 colorNormal_ = { 0.2f, 0.6f, 1.0f, 1.0f };
    Vector4 colorDashed_ = { 1.0f, 1.0f, 1.0f, 1.0f };
    float wallJumpDuration_ = 0.5f;
    Vector2 wallJumpPower_ = { 8.0f, 12.0f };
    float wallJumpDirLockDuration_ = 0.4f;
    float wallSlideSpeed_ = -2.0f;
    float wallClimbSpeed_ = 5.0f;
    float wallClingReleaseDuration_ = 0.5f;
    float halfWidth_ = 0.4f;
    float halfHeight_ = 0.8f;
    float deathDuration_ = 0.175f;
    float respawnDuration_ = 0.5f;
    float goalWaitTime_ = 2.0f;
    float runDustInterval_ = 0.1f;
    
    float maxStamina_ = 110.0f;
    float staminaConsumeCling_ = 10.0f;
    float staminaConsumeClimb_ = 45.0f;
    float staminaConsumeJump_ = 27.5f;
    Vector4 colorTired_ = { 0.8f, 0.2f, 0.2f, 1.0f };
};

class PlayerConfig {
public:
    static void Save(const PlayerParams& params, const std::string& filepath);
    static void Load(PlayerParams& params, const std::string& filepath);
};
