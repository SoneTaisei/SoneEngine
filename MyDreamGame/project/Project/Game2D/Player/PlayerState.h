#pragma once
#include "Core/Utility/Structs.h"
struct PlayerState {
    Vector3 position_ = { 2.0f, 5.0f, 0.0f };
    Vector3 velocity_ = { 0.0f, 0.0f, 0.0f };
    float launchVelocityX_ = 0.0f;                    // 鎖アクション(スピンジャンプ等)の発射横速度。着地・壁接触で消える
    Vector3 startPosition_ = { 2.0f, 5.0f, 0.0f };
    Vector3 platformVelocity_ = { 0.0f, 0.0f, 0.0f }; // 乗っている床の速度
    bool isOnGround_ = false;

    // ゲーム進行・ステート
    bool isDead_ = false;
    float deathTimer_ = 0.0f;
    bool isRespawning_ = false;
    float respawnTimer_ = 0.0f;
    bool isGoal_ = false;
    float goalTimer_ = 0.0f;
    bool isClearEscaped_ = false; // クリア演出で煙幕に紛れて脱出・消失したフラグ

    // 鎖の長さ（個数）
    int chainLength_ = 3;

    // 見た目・エフェクト同期用フラグ
    bool isDashing_ = false;
    bool isHoldingChain_ = false;
    bool isSwingingChain_ = false;
    float chainSwingOmega_ = 0.0f;
    float chainSwingTheta_ = 0.0f;
    float spinFlipTimer_ = 0.0f;
    float spinFlipDuration_ = 0.45f;
    float spinFlipSign_ = -1.0f;
    float faceDirX_ = 0.0f;       // 見た目の向きの指定（-1 左 / +1 右 / 0 は速度まかせ）。投げた時など、動かないまま向きだけ変える用
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
