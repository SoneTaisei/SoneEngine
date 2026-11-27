#include "PlayerDash.h"
#include "../Player2D.h"
#include "Core/Utility/TransformFunctions.h"
#include "Graphics/Camera/GameCamera.h"
#include <cmath>

void PlayerDash::TryStartDash(PlayerState& state, const PlayerParams& params, const InputState& input, PlayerVisuals& visuals, Player2D* player) {
    if (!state.canDash_ || !input.isDashPressed || state.climbLandingTimer_ > 0.0f) {
        return;
    }

    Vector3 inputDir = { 0.0f, 0.0f, 0.0f };
    if (input.moveX < 0.0f) inputDir.x -= 1.0f;
    if (input.moveX > 0.0f) inputDir.x += 1.0f;
    if (input.moveY > 0.5f) inputDir.y += 1.0f;
    if (input.moveY < -0.5f) inputDir.y -= 1.0f;

    if (inputDir.x == 0.0f && inputDir.y == 0.0f) {
        inputDir.x = (state.facingDirectionX_ != 0.0f) ? state.facingDirectionX_ : 1.0f;
    } else {
        inputDir = TransformFunctions::Normalize(inputDir);
    }

    if (inputDir.x > 0.01f) {
        state.facingDirectionX_ = 1.0f;
    } else if (inputDir.x < -0.01f) {
        state.facingDirectionX_ = -1.0f;
    }

    state.dashVelocity_ = { inputDir.x * params.dashSpeed_, inputDir.y * params.dashSpeed_, 0.0f };
    state.velocity_ = state.dashVelocity_;
    state.isDashing_ = true;
    state.isJumping_ = false;
    state.minJumpTimer_ = 0.0f;
    state.canDash_ = false;
    state.dashTimer_ = 0.0f;
    state.hasDashInertia_ = false;
    state.lastDashDirX_ = (inputDir.x > 0.01f) ? 1.0f : ((inputDir.x < -0.01f) ? -1.0f : 0.0f);
    state.dashJumpWindowTimer_ = params.dashDuration_ + params.dashJumpWindow_;

    visuals.SpawnDashRing(state.position_, inputDir);

    if (player && player->GetCamera()) {
        player->GetCamera()->Shake(0.2f, 0.15f);
    }
}

void PlayerDash::UpdateDashTimer(PlayerState& state, const PlayerParams& params, float deltaTime) {
    if (!state.isDashing_) {
        return;
    }

    state.dashTimer_ += deltaTime;
    if (state.dashTimer_ >= params.dashDuration_) {
        state.isDashing_ = false;
        if (std::abs(state.dashVelocity_.x) > 0.01f) {
            state.velocity_.x = state.dashVelocity_.x * params.dashEndInertiaRatio_;
            state.hasDashInertia_ = true;
        } else {
            state.hasDashInertia_ = false;
        }
        if (state.velocity_.y > params.dashEndUpwardVelocity_) {
            state.velocity_.y = params.dashEndUpwardVelocity_;
        }
    } else {
        state.velocity_ = state.dashVelocity_;
    }
}
