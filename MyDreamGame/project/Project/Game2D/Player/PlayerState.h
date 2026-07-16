#pragma once
#include "Core/Utility/Structs.h"
struct PlayerState {
    class ColliderComponent* standingPlatformCollider_ = nullptr;
    class ColliderComponent* wallPlatformCollider_ = nullptr;
    
    Vector3 position_ = { 2.0f, 5.0f, 0.0f };
    Vector3 velocity_ = { 0.0f, 0.0f, 0.0f };
    bool isOnMovingPlatform_ = false;
    Vector3 platformVelocity_ = { 0.0f, 0.0f, 0.0f };
    Vector3 recentPlatformVelocity_ = { 0.0f, 0.0f, 0.0f };
    Vector3 wallPlatformVelocity_ = { 0.0f, 0.0f, 0.0f };
    float platformInertiaTimer_ = 0.0f;
    float externalVelocityX_ = 0.0f;
    bool isOnGround_ = false;
    bool canDash_ = true;
    bool isDashing_ = false;
    float dashTimer_ = 0.0f;
    bool isTouchingWallRight_ = false;
    bool isTouchingWallLeft_ = false;
    bool wasTouchingWallRight_ = false;
    bool wasTouchingWallLeft_ = false;
    float wallJumpTimer_ = 0.0f;
    float wallJumpDirLockTimer_ = 0.0f;
    Vector3 dashVelocity_ = {0.0f, 0.0f, 0.0f};
    float lockedDirectionX_ = 0.0f;
    bool isWallSliding_ = false;
    bool isWallClinging_ = false;
    float wallClingReleaseTimer_ = 0.0f;
    bool isDead_ = false;
    float deathTimer_ = 0.0f;
    Vector3 startPosition_ = { 2.0f, 5.0f, 0.0f };
    int currentRoomIndex_ = -1;
    bool isRespawning_ = false;
    float respawnTimer_ = 0.0f;
    bool isGoal_ = false;
    float goalTimer_ = 0.0f;
    int score_ = 0;
    float runDustTimer_ = 0.0f;
    float stuckTimer_ = 0.0f;
    Vector3 prevPositionForBugCheck_ = { 0.0f, 0.0f, 0.0f };
    float inWallTimer_ = 0.0f;
    float stamina_ = 110.0f;
    bool isExhausted_ = false;
};
