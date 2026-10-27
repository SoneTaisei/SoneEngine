#include "ReadyGoUI.h"
#include "Resource/Sprite/Sprite.h"
#include "Graphics/TextureManager.h"
#include "Core/Utility/ParameterManager.h"
#include <cmath>
#include <algorithm>

void ReadyGoUI::Initialize(SpriteCommon* spriteCommon) {
    if (!spriteCommon) return;

    readyTextureHandle_ = TextureManager::GetInstance()->Load("resources/Sprite/Original/UI/ready.png");
    goTextureHandle_ = TextureManager::GetInstance()->Load("resources/Sprite/Original/UI/go.png");
    whiteTextureHandle_ = TextureManager::GetInstance()->Load("resources/Sprite/Original/UI/white.png");

    float centerX = 1280.0f * 0.5f;

    readySprite_ = std::make_unique<Sprite>();
    readySprite_->Initialize(spriteCommon, readyTextureHandle_);
    float readyScale = ParameterManager::GetInstance()->GetValue("GameScene", "readySpriteScale", 2.0f);
    float readyW = 240.0f * readyScale;
    float readyH = 92.0f * readyScale;
    float centerY = ParameterManager::GetInstance()->GetValue("GameScene", "readySpriteCenterY", 360.0f);
    readySprite_->SetPosition({ centerX - readyW * 0.5f, centerY - readyH * 0.5f });
    readySprite_->SetSize({ readyW, readyH });
    readySprite_->SetColor({ 1.0f, 0.15f, 0.15f, 0.0f }); // 赤色

    goSprite_ = std::make_unique<Sprite>();
    goSprite_->Initialize(spriteCommon, goTextureHandle_);
    float goScale = ParameterManager::GetInstance()->GetValue("GameScene", "goSpriteScale", 2.0f);
    float goW = 144.0f * goScale;
    float goH = 92.0f * goScale;
    float goCenterY = ParameterManager::GetInstance()->GetValue("GameScene", "goSpriteCenterY", 360.0f);
    goSprite_->SetPosition({ centerX - goW * 0.5f, goCenterY - goH * 0.5f });
    goSprite_->SetSize({ goW, goH });
    goSprite_->SetColor({ 1.0f, 0.95f, 0.1f, 0.0f }); // 黄色

    // タイムバー (細長い四角形)
    float barWidth = ParameterManager::GetInstance()->GetValue("GameScene", "readyBarWidth", 360.0f);
    float barHeight = ParameterManager::GetInstance()->GetValue("GameScene", "readyBarHeight", 8.0f);
    float barOffsetY = ParameterManager::GetInstance()->GetValue("GameScene", "readyBarOffsetY", 110.0f);
    float barY = centerY + barOffsetY;

    readyBarBgSprite_ = std::make_unique<Sprite>();
    readyBarBgSprite_->Initialize(spriteCommon, whiteTextureHandle_);
    readyBarBgSprite_->SetPosition({ centerX - barWidth * 0.5f - 2.0f, barY - 2.0f });
    readyBarBgSprite_->SetSize({ barWidth + 4.0f, barHeight + 4.0f });
    readyBarBgSprite_->SetColor({ 0.1f, 0.1f, 0.1f, 0.0f });

    readyBarFillSprite_ = std::make_unique<Sprite>();
    readyBarFillSprite_->Initialize(spriteCommon, whiteTextureHandle_);
    readyBarFillSprite_->SetPosition({ centerX - barWidth * 0.5f, barY });
    readyBarFillSprite_->SetSize({ barWidth, barHeight });
    readyBarFillSprite_->SetColor({ 1.0f, 0.15f, 0.15f, 0.0f });

    Reset();
}

void ReadyGoUI::Start() {
    isActive_ = true;
    timer_ = 0.0f;
}

void ReadyGoUI::Reset() {
    isActive_ = true;
    timer_ = 0.0f;
    if (readySprite_) readySprite_->SetColor({ 1.0f, 0.15f, 0.15f, 0.0f });
    if (readyBarBgSprite_) readyBarBgSprite_->SetColor({ 0.1f, 0.1f, 0.1f, 0.0f });
    if (readyBarFillSprite_) readyBarFillSprite_->SetColor({ 1.0f, 0.2f, 0.2f, 0.0f });
    if (goSprite_) goSprite_->SetColor({ 1.0f, 0.95f, 0.1f, 0.0f });
}

bool ReadyGoUI::CanControl() const {
    if (!isActive_) return true;
    float inputDelay = ParameterManager::GetInstance()->GetValue("GameScene", "startReadyInputDelay", 1.0f);
    return timer_ >= inputDelay;
}

bool ReadyGoUI::Update(float deltaTime) {
    if (!isActive_) return true;

    timer_ += deltaTime;
    float startReadyTime = ParameterManager::GetInstance()->GetValue("GameScene", "startReadyTime", 2.0f);
    float inputDelay = ParameterManager::GetInstance()->GetValue("GameScene", "startReadyInputDelay", 1.0f);
    float centerX = 1280.0f * 0.5f;

    if (timer_ < inputDelay) {
        // READY... 演出
        float alpha = (std::min)(timer_ / 0.15f, 1.0f);
        float centerY = ParameterManager::GetInstance()->GetValue("GameScene", "readySpriteCenterY", 360.0f);

        if (readySprite_) {
            float appearDuration = 0.3f;
            float progress = (std::clamp)(timer_ / appearDuration, 0.0f, 1.0f);
            float c1 = 1.70158f;
            float c3 = c1 + 1.0f;
            float ease = 1.0f + c3 * std::pow(progress - 1.0f, 3.0f) + c1 * std::pow(progress - 1.0f, 2.0f);
            float scaleFactor = (std::max)(ease, 0.0f);

            float readyScale = ParameterManager::GetInstance()->GetValue("GameScene", "readySpriteScale", 2.0f);
            float baseW = 240.0f * readyScale;
            float baseH = 92.0f * readyScale;
            float currentW = baseW * scaleFactor;
            float currentH = baseH * scaleFactor;

            readySprite_->SetPosition({ centerX - currentW * 0.5f, centerY - currentH * 0.5f });
            readySprite_->SetSize({ currentW, currentH });
            readySprite_->SetColor({ 1.0f, 0.15f, 0.15f, alpha });
            readySprite_->Update();
        }

        // タイムバー演出 (レディー表示残り時間を細長い棒で表示)
        float barWidth = ParameterManager::GetInstance()->GetValue("GameScene", "readyBarWidth", 360.0f);
        float barHeight = ParameterManager::GetInstance()->GetValue("GameScene", "readyBarHeight", 8.0f);
        float barOffsetY = ParameterManager::GetInstance()->GetValue("GameScene", "readyBarOffsetY", 110.0f);
        float barY = centerY + barOffsetY;
        float remainRatio = (std::clamp)((inputDelay - timer_) / inputDelay, 0.0f, 1.0f);
        float currentFillW = barWidth * remainRatio;

        if (readyBarBgSprite_) {
            readyBarBgSprite_->SetPosition({ centerX - barWidth * 0.5f - 2.0f, barY - 2.0f });
            readyBarBgSprite_->SetSize({ barWidth + 4.0f, barHeight + 4.0f });
            readyBarBgSprite_->SetColor({ 0.1f, 0.1f, 0.1f, 0.6f * alpha });
            readyBarBgSprite_->Update();
        }
        if (readyBarFillSprite_) {
            readyBarFillSprite_->SetPosition({ centerX - barWidth * 0.5f, barY });
            readyBarFillSprite_->SetSize({ currentFillW, barHeight });
            readyBarFillSprite_->SetColor({ 1.0f, 0.2f, 0.2f, alpha });
            readyBarFillSprite_->Update();
        }

        if (goSprite_) {
            goSprite_->SetColor({ 1.0f, 0.95f, 0.1f, 0.0f });
        }
    } else {
        // GO! 演出
        if (readySprite_) {
            readySprite_->SetColor({ 1.0f, 0.15f, 0.15f, 0.0f });
        }
        if (readyBarBgSprite_) {
            readyBarBgSprite_->SetColor({ 0.1f, 0.1f, 0.1f, 0.0f });
        }
        if (readyBarFillSprite_) {
            readyBarFillSprite_->SetColor({ 1.0f, 0.2f, 0.2f, 0.0f });
        }

        if (goSprite_) {
            float goTimer = timer_ - inputDelay;
            float appearDuration = 0.25f;
            float progress = (std::clamp)(goTimer / appearDuration, 0.0f, 1.0f);
            float c1 = 1.70158f;
            float c3 = c1 + 1.0f;
            float ease = 1.0f + c3 * std::pow(progress - 1.0f, 3.0f) + c1 * std::pow(progress - 1.0f, 2.0f);
            float scaleFactor = (std::max)(ease, 0.0f);

            float goScale = ParameterManager::GetInstance()->GetValue("GameScene", "goSpriteScale", 2.0f);
            float baseW = 144.0f * goScale;
            float baseH = 92.0f * goScale;
            float currentW = baseW * scaleFactor;
            float currentH = baseH * scaleFactor;
            float centerY = ParameterManager::GetInstance()->GetValue("GameScene", "goSpriteCenterY", 360.0f);

            goSprite_->SetPosition({ centerX - currentW * 0.5f, centerY - currentH * 0.5f });
            goSprite_->SetSize({ currentW, currentH });

            float alpha = 1.0f;
            float fadeStart = startReadyTime - 0.25f;
            if (timer_ > fadeStart) {
                alpha = (std::max)(0.0f, (startReadyTime - timer_) / 0.25f);
            }
            goSprite_->SetColor({ 1.0f, 0.95f, 0.1f, alpha });
            goSprite_->Update();
        }
    }

    if (timer_ > startReadyTime) {
        isActive_ = false;
        timer_ = 0.0f;
        if (readySprite_) readySprite_->SetColor({ 1.0f, 0.15f, 0.15f, 0.0f });
        if (readyBarBgSprite_) readyBarBgSprite_->SetColor({ 0.1f, 0.1f, 0.1f, 0.0f });
        if (readyBarFillSprite_) readyBarFillSprite_->SetColor({ 1.0f, 0.2f, 0.2f, 0.0f });
        if (goSprite_) goSprite_->SetColor({ 1.0f, 0.95f, 0.1f, 0.0f });
        return true;
    }

    return false;
}

void ReadyGoUI::Draw() {
    if (!isActive_) return;

    float inputDelay = ParameterManager::GetInstance()->GetValue("GameScene", "startReadyInputDelay", 1.0f);
    if (timer_ < inputDelay) {
        if (readyBarBgSprite_) readyBarBgSprite_->Draw();
        if (readyBarFillSprite_) readyBarFillSprite_->Draw();
        if (readySprite_) readySprite_->Draw();
    } else {
        if (goSprite_) goSprite_->Draw();
    }
}
