#include "ClearUI.h"
#include "Resource/Sprite/Sprite.h"
#include "Graphics/TextureManager.h"
#include "Graphics/Camera/GameCamera.h"
#include "Input/KeyboardInput.h"
#include "Input/GamepadInput.h"
#include "Core/Utility/ParameterManager.h"
#include <cmath>
#include <algorithm>

void ClearUI::Initialize(SpriteCommon* spriteCommon) {
    if (!spriteCommon) return;

    clearTextureHandle_ = TextureManager::GetInstance()->Load("resources/Sprite/Original/UI/clear.png");
    clearSprite_ = std::make_unique<Sprite>();
    clearSprite_->Initialize(spriteCommon, clearTextureHandle_);

    float clearScale = ParameterManager::GetInstance()->GetValue("GameScene", "clearSpriteScale", 2.0f);
    float spriteW = 220.0f * clearScale;
    float spriteH = 92.0f * clearScale;
    float centerX = 1280.0f * 0.5f;
    float centerY = ParameterManager::GetInstance()->GetValue("GameScene", "clearSpriteCenterY", 180.0f);

    clearSprite_->SetPosition({ centerX - spriteW * 0.5f, centerY - spriteH * 0.5f });
    clearSprite_->SetSize({ spriteW, spriteH });
    clearSprite_->SetColor({ 1.0f, 1.0f, 1.0f, 0.0f });

    Reset();
}

void ClearUI::Start(float initialCameraScale) {
    isActive_ = true;
    timer_ = 0.0f;
    initialCameraScale_ = initialCameraScale;
}

void ClearUI::Reset(GameCamera* camera) {
    isActive_ = false;
    timer_ = 0.0f;
    if (clearSprite_) {
        clearSprite_->SetColor({ 1.0f, 1.0f, 1.0f, 0.0f });
    }
    if (camera) {
        if (std::abs(camera->GetScale() - initialCameraScale_) > 0.001f) {
            camera->SetScale(initialCameraScale_);
        }
        if (std::abs(camera->GetFollowOffset().y) > 0.001f) {
            camera->SetFollowOffset({ 0.0f, 0.0f, 0.0f });
        }
    }
}

void ClearUI::Update(float deltaTime, GameCamera* camera) {
    if (!isActive_) {
        if (clearSprite_) {
            clearSprite_->SetColor({ 1.0f, 1.0f, 1.0f, 0.0f });
        }
        if (camera) {
            if (std::abs(camera->GetScale() - initialCameraScale_) > 0.001f) {
                camera->SetScale(initialCameraScale_);
            }
            if (std::abs(camera->GetFollowOffset().y) > 0.001f) {
                camera->SetFollowOffset({ 0.0f, 0.0f, 0.0f });
            }
        }
        return;
    }

    timer_ += deltaTime;

    if (clearSprite_) {
        // 文字が遠くからズームして出てくる演出 (EaseOutBack)
        float appearDuration = ParameterManager::GetInstance()->GetValue("GameScene", "clearSpriteAppearDuration", 0.6f);
        float progress = (std::clamp)(timer_ / appearDuration, 0.0f, 1.0f);

        float c1 = 1.70158f;
        float c3 = c1 + 1.0f;
        float ease = 1.0f + c3 * std::pow(progress - 1.0f, 3.0f) + c1 * std::pow(progress - 1.0f, 2.0f);
        float scaleFactor = (std::max)(ease, 0.0f);

        float clearScale = ParameterManager::GetInstance()->GetValue("GameScene", "clearSpriteScale", 2.0f);
        float baseW = 220.0f * clearScale;
        float baseH = 92.0f * clearScale;
        float currentW = baseW * scaleFactor;
        float currentH = baseH * scaleFactor;

        float centerX = 1280.0f * 0.5f;
        float targetCenterY = ParameterManager::GetInstance()->GetValue("GameScene", "clearSpriteCenterY", 180.0f);

        clearSprite_->SetPosition({ centerX - currentW * 0.5f, targetCenterY - currentH * 0.5f });
        clearSprite_->SetSize({ currentW, currentH });

        float alpha = (std::min)(timer_ / 0.2f, 1.0f);
        clearSprite_->SetColor({ 1.0f, 1.0f, 1.0f, alpha });
        clearSprite_->Update();
    }

    if (camera) {
        // クリア時はプレイヤーへカメラをズームイン（アップ）
        float targetScale = ParameterManager::GetInstance()->GetValue("GameScene", "clearCameraZoomScale", 2.2f);
        float zoomSpeed = ParameterManager::GetInstance()->GetValue("GameScene", "clearCameraZoomSpeed", 3.0f);
        float currentScale = camera->GetScale();
        float newScale = currentScale + (targetScale - currentScale) * (1.0f - std::exp(-zoomSpeed * deltaTime));
        camera->SetScale(newScale);

        // プレイヤーを画面のやや下側に配置するため、カメラの注視点を上方にオフセット
        float targetOffsetY = ParameterManager::GetInstance()->GetValue("GameScene", "clearCameraOffsetY", 1.0f);
        Vector3 currentOffset = camera->GetFollowOffset();
        float newOffsetY = currentOffset.y + (targetOffsetY - currentOffset.y) * (1.0f - std::exp(-zoomSpeed * deltaTime));
        camera->SetFollowOffset({ currentOffset.x, newOffsetY, currentOffset.z });
    }
}

bool ClearUI::IsReturnRequested() const {
    if (!isActive_) return false;

    return KeyboardInput::GetInstance()->IsKeyPressed(DIK_SPACE) ||
           GamepadInput::GetInstance()->IsButtonPressed(GamepadButton::A) ||
           GamepadInput::GetInstance()->IsButtonPressed(GamepadButton::Start);
}

void ClearUI::Draw() {
    if (!isActive_) return;

    if (clearSprite_) {
        clearSprite_->Draw();
    }
}
