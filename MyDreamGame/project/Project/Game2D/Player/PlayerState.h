#pragma once
#include "Core/Utility/Structs.h"
struct PlayerState {
    Vector3 position_ = { 2.0f, 5.0f, 0.0f };
    Vector3 velocity_ = { 0.0f, 0.0f, 0.0f };
    Vector3 startPosition_ = { 2.0f, 5.0f, 0.0f };
    bool isOnGround_ = false;

    // ゲーム進行・ステート
    bool isDead_ = false;
    float deathTimer_ = 0.0f;
    bool isRespawning_ = false;
    float respawnTimer_ = 0.0f;
    bool isGoal_ = false;
    float goalTimer_ = 0.0f;

    // 見た目・エフェクト同期用フラグ
    bool isDashing_ = false;
    bool isWallClinging_ = false;
    bool isWallSliding_ = false;
    bool isTouchingWallLeft_ = false;
    bool isTouchingWallRight_ = false;
    float stamina_ = 110.0f;
    bool isExhausted_ = false;
    float runDustTimer_ = 0.0f;

    int currentRoomIndex_ = -1;
    float stuckTimer_ = 0.0f;
    Vector3 prevPositionForBugCheck_ = { 0.0f, 0.0f, 0.0f };
};
