#include "StaminaHUD.h"
#include "Core/TimeManager.h"
#include "Core/Utility/ParameterManager.h"
#include "Core/Utility/TransformFunctions.h"
#include "Game2D/Player/Player2D.h"
#include "Graphics/CameraManager.h"
#include "Graphics/TextureManager.h"
#include "Resource/Sprite/Sprite.h"
#include "Resource/Sprite/SpriteCommon.h"
#include <algorithm>
#include <cmath>

void StaminaHUD::Initialize(SpriteCommon* spriteCommon) {
    spriteCommon_ = spriteCommon;
    if (!spriteCommon_) {
        return;
    }

    whiteTextureHandle_ = TextureManager::GetInstance()->Load("resources/Sprite/Original/UI/white.png");

    bgBorderSprite_ = std::make_unique<Sprite>();
    bgBorderSprite_->Initialize(spriteCommon_, whiteTextureHandle_);
    bgBorderSprite_->SetColor({0.05f, 0.06f, 0.09f, 0.0f});

    bgTrackSprite_ = std::make_unique<Sprite>();
    bgTrackSprite_->Initialize(spriteCommon_, whiteTextureHandle_);
    bgTrackSprite_->SetColor({0.16f, 0.18f, 0.22f, 0.0f});

    fillSprite_ = std::make_unique<Sprite>();
    fillSprite_->Initialize(spriteCommon_, whiteTextureHandle_);
    fillSprite_->SetColor({0.18f, 0.90f, 0.42f, 0.0f});

    Reset();
}

void StaminaHUD::Reset() {
    alpha_ = 0.0f;
    currentSide_ = -1.0f;
    blinkTimer_ = 0.0f;
    displayRatio_ = 1.0f;
}

void StaminaHUD::Update() {
    if (!player_) {
        alpha_ = 0.0f;
        return;
    }

    const float deltaTime = TimeManager::GetInstance().GetDeltaTime();
    blinkTimer_ += deltaTime;

    const PlayerState& state = player_->GetState();
    const PlayerParams& params = player_->GetParams();

    if (state.isDead_ || state.isRespawning_ || state.isGoal_ || params.maxStamina_ <= 0.0f) {
        alpha_ = 0.0f;
        return;
    }

    const float ratio = std::clamp(state.stamina_ / params.maxStamina_, 0.0f, 1.0f);
    // 表示値は素早く追従させて滑らかに見せる
    const float ratioLerp = (std::min)(1.0f, deltaTime * 25.0f);
    displayRatio_ += (ratio - displayRatio_) * ratioLerp;

    // 壁掴み中・疲労中・スタミナ消費中（回復前の空中など）に表示する
    const bool isStaminaUsed = (state.stamina_ < params.maxStamina_ - 0.05f);
    const bool shouldShow = state.isWallClinging_ || state.isExhausted_ || isStaminaUsed;

    if (shouldShow) {
        alpha_ = (std::min)(1.0f, alpha_ + deltaTime * 10.0f);
    } else {
        alpha_ = (std::max)(0.0f, alpha_ - deltaTime * 5.0f);
    }

    // 壁の反対側（開けている側）に配置方向を決定する
    float targetSide = currentSide_;
    if (state.isTouchingWallRight_ && !state.isTouchingWallLeft_) {
        targetSide = -1.0f;
    } else if (state.isTouchingWallLeft_ && !state.isTouchingWallRight_) {
        targetSide = 1.0f;
    } else if (!state.isTouchingWallRight_ && !state.isTouchingWallLeft_) {
        if (alpha_ <= 0.01f) {
            targetSide = (state.facingDirectionX_ >= 0.0f) ? -1.0f : 1.0f;
        }
    }

    if (alpha_ <= 0.05f) {
        currentSide_ = targetSide;
    } else {
        const float sideLerp = (std::min)(1.0f, deltaTime * 15.0f);
        currentSide_ += (targetSide - currentSide_) * sideLerp;
    }
}

bool StaminaHUD::WorldToSpritePos(const Vector3& worldPos, Vector2& outSpritePos) const {
    if (!spriteCommon_) {
        return false;
    }

    CameraManager* cameraMgr = CameraManager::GetInstance();
    if (!cameraMgr) {
        return false;
    }

    const Matrix4x4 vp = TransformFunctions::Multiply(cameraMgr->GetViewMatrix(), cameraMgr->GetProjectionMatrix());
    const float cx = worldPos.x * vp.m[0][0] + worldPos.y * vp.m[1][0] + worldPos.z * vp.m[2][0] + vp.m[3][0];
    const float cy = worldPos.x * vp.m[0][1] + worldPos.y * vp.m[1][1] + worldPos.z * vp.m[2][1] + vp.m[3][1];
    const float cw = worldPos.x * vp.m[0][3] + worldPos.y * vp.m[1][3] + worldPos.z * vp.m[2][3] + vp.m[3][3];

    if (cw <= 0.0001f) {
        return false;
    }

    const float ndcX = cx / cw;
    const float ndcY = cy / cw;

    const Matrix4x4 spriteVP = TransformFunctions::Multiply(spriteCommon_->GetViewMatrix(), spriteCommon_->GetProjectionMatrix());
    const Matrix4x4 invSpriteVP = TransformFunctions::Inverse(spriteVP);

    const float sw = ndcX * invSpriteVP.m[0][3] + ndcY * invSpriteVP.m[1][3] + invSpriteVP.m[3][3];
    if (std::abs(sw) <= 0.0001f) {
        return false;
    }

    outSpritePos.x = (ndcX * invSpriteVP.m[0][0] + ndcY * invSpriteVP.m[1][0] + invSpriteVP.m[3][0]) / sw;
    outSpritePos.y = (ndcX * invSpriteVP.m[0][1] + ndcY * invSpriteVP.m[1][1] + invSpriteVP.m[3][1]) / sw;
    return true;
}

Vector4 StaminaHUD::ComputeFillColor(float ratio, bool isExhausted) const {
    if (isExhausted || ratio <= 0.2f) {
        const float pulse = 0.5f + 0.5f * std::sin(blinkTimer_ * 24.0f);
        return {0.95f, 0.16f + 0.18f * pulse, 0.16f + 0.12f * pulse, alpha_};
    }
    if (ratio <= 0.5f) {
        const float t = (ratio - 0.2f) / 0.3f; // 0 (20%) -> 1 (50%)
        return {1.0f, 0.45f + 0.38f * t, 0.12f, alpha_};
    }
    const float t = (ratio - 0.5f) / 0.5f; // 0 (50%) -> 1 (100%)
    return {0.95f - 0.77f * t, 0.85f + 0.07f * t, 0.18f + 0.27f * t, alpha_};
}

void StaminaHUD::Draw() {
    if (!spriteCommon_ || !player_ || alpha_ <= 0.001f || !bgBorderSprite_ || !bgTrackSprite_ || !fillSprite_) {
        return;
    }

    const PlayerState& state = player_->GetState();
    const PlayerParams& params = player_->GetParams();
    if (state.isDead_ || state.isRespawning_ || state.isGoal_ || params.maxStamina_ <= 0.0f) {
        return;
    }

    auto* paramMgr = ParameterManager::GetInstance();
    const float baseBarWidth = paramMgr->GetValue("StaminaUI", "barWidth", 8.0f);
    const float baseBarHeight = paramMgr->GetValue("StaminaUI", "barHeight", 52.0f);
    const float baseBorder = paramMgr->GetValue("StaminaUI", "borderSize", 2.0f);
    const float worldOffsetX = paramMgr->GetValue("StaminaUI", "worldOffsetX", 0.95f);
    const float worldOffsetY = paramMgr->GetValue("StaminaUI", "worldOffsetY", 0.05f);

    const Vector3 anchorWorld = {
        state.position_.x + currentSide_ * worldOffsetX,
        state.position_.y + worldOffsetY,
        state.position_.z
    };
    const Vector3 playerTopWorld = {state.position_.x, state.position_.y + params.halfHeight_, state.position_.z};
    const Vector3 playerBottomWorld = {state.position_.x, state.position_.y - params.halfHeight_, state.position_.z};

    Vector2 anchorScreen{};
    Vector2 topScreen{};
    Vector2 bottomScreen{};
    if (!WorldToSpritePos(anchorWorld, anchorScreen) ||
        !WorldToSpritePos(playerTopWorld, topScreen) ||
        !WorldToSpritePos(playerBottomWorld, bottomScreen)) {
        return;
    }

    const float playerPixelHeight = std::abs(bottomScreen.y - topScreen.y);
    const float uiScale = std::clamp(playerPixelHeight / 56.0f, 0.75f, 2.0f);

    const float barWidth = baseBarWidth * uiScale;
    const float barHeight = baseBarHeight * uiScale;
    const float border = baseBorder * uiScale;
    const float totalW = barWidth + border * 2.0f;
    const float totalH = barHeight + border * 2.0f;

    // 画面表示範囲の取得（アスペクト比補正を考慮してクランプ）
    Vector2 screenMin{};
    Vector2 screenMax{};
    const Matrix4x4 invSpriteVP = TransformFunctions::Inverse(
        TransformFunctions::Multiply(spriteCommon_->GetViewMatrix(), spriteCommon_->GetProjectionMatrix()));
    screenMin.x = -1.0f * invSpriteVP.m[0][0] + 1.0f * invSpriteVP.m[1][0] + invSpriteVP.m[3][0];
    screenMin.y = -1.0f * invSpriteVP.m[0][1] + 1.0f * invSpriteVP.m[1][1] + invSpriteVP.m[3][1];
    screenMax.x = 1.0f * invSpriteVP.m[0][0] - 1.0f * invSpriteVP.m[1][0] + invSpriteVP.m[3][0];
    screenMax.y = 1.0f * invSpriteVP.m[0][1] - 1.0f * invSpriteVP.m[1][1] + invSpriteVP.m[3][1];
    const float minX = (std::min)(screenMin.x, screenMax.x) + 8.0f;
    const float maxX = (std::max)(screenMin.x, screenMax.x) - 8.0f - totalW;
    const float minY = (std::min)(screenMin.y, screenMax.y) + 8.0f;
    const float maxY = (std::max)(screenMin.y, screenMax.y) - 8.0f - totalH;

    float outerX = anchorScreen.x - totalW * 0.5f;
    float outerY = anchorScreen.y - totalH * 0.5f;
    if (maxX >= minX) {
        outerX = std::clamp(outerX, minX, maxX);
    }
    if (maxY >= minY) {
        outerY = std::clamp(outerY, minY, maxY);
    }

    const float innerX = outerX + border;
    const float innerY = outerY + border;

    // 疲労時は枠と背景を赤く点滅させて空になったことを分かりやすくする
    Vector4 borderColor = {0.05f, 0.06f, 0.09f, 0.90f * alpha_};
    Vector4 trackColor = {0.16f, 0.18f, 0.22f, 0.85f * alpha_};
    if (state.isExhausted_) {
        const float flash = 0.5f + 0.5f * std::sin(blinkTimer_ * 26.0f);
        borderColor = {0.55f + 0.35f * flash, 0.08f, 0.08f, 0.95f * alpha_};
        trackColor = {0.28f + 0.15f * flash, 0.08f, 0.08f, 0.88f * alpha_};
    }

    bgBorderSprite_->SetPosition({outerX, outerY});
    bgBorderSprite_->SetSize({totalW, totalH});
    bgBorderSprite_->SetColor(borderColor);
    bgBorderSprite_->Update();
    bgBorderSprite_->Draw();

    bgTrackSprite_->SetPosition({innerX, innerY});
    bgTrackSprite_->SetSize({barWidth, barHeight});
    bgTrackSprite_->SetColor(trackColor);
    bgTrackSprite_->Update();
    bgTrackSprite_->Draw();

    const float clampedRatio = std::clamp(displayRatio_, 0.0f, 1.0f);
    const float fillHeight = barHeight * clampedRatio;
    if (fillHeight > 0.5f) {
        const float fillY = innerY + (barHeight - fillHeight);
        fillSprite_->SetPosition({innerX, fillY});
        fillSprite_->SetSize({barWidth, fillHeight});
        fillSprite_->SetColor(ComputeFillColor(clampedRatio, state.isExhausted_));
        fillSprite_->Update();
        fillSprite_->Draw();
    }
}
