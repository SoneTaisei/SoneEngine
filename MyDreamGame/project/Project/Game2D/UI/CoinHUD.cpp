#include "CoinHUD.h"
#include "Resource/Sprite/Sprite.h"
#include "Graphics/TextureManager.h"
#include "Core/Utility/ParameterManager.h"
#include <cmath>
#include <string>

void CoinHUD::Initialize(SpriteCommon* spriteCommon) {
    if (!spriteCommon) return;

    coinTextureHandle_ = TextureManager::GetInstance()->Load("resources/Sprite/Original/UI/coin.png");
    numbersTextureHandle_ = TextureManager::GetInstance()->Load("resources/Sprite/Original/UI/numbers.png");

    coinSprite_ = std::make_unique<Sprite>();
    coinSprite_->Initialize(spriteCommon, coinTextureHandle_);
    coinSprite_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });

    numberSprites_.clear();
    for (int i = 0; i < 16; ++i) {
        auto numSprite = std::make_unique<Sprite>();
        numSprite->Initialize(spriteCommon, numbersTextureHandle_);
        numSprite->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
        numberSprites_.push_back(std::move(numSprite));
    }

    coinPopAnimTimer_ = 0.0f;
    lastCollectedCoins_ = 0;
    currentCollectedCoins_ = 0;
    currentTotalCoins_ = 0;
}

void CoinHUD::Reset() {
    coinPopAnimTimer_ = 0.0f;
    lastCollectedCoins_ = 0;
    currentCollectedCoins_ = 0;
    currentTotalCoins_ = 0;
}

void CoinHUD::Update(float deltaTime, int collectedCoins, int totalCoins) {
    currentCollectedCoins_ = collectedCoins;
    currentTotalCoins_ = totalCoins;

    if (collectedCoins > lastCollectedCoins_) {
        coinPopAnimTimer_ = 0.35f;
    }
    lastCollectedCoins_ = collectedCoins;

    if (coinPopAnimTimer_ > 0.0f) {
        coinPopAnimTimer_ -= deltaTime;
        if (coinPopAnimTimer_ < 0.0f) {
            coinPopAnimTimer_ = 0.0f;
        }
    }
}

void CoinHUD::Draw() {
    if (!coinSprite_ || currentTotalCoins_ <= 0) return;

    std::string text = std::to_string(currentCollectedCoins_) + "/" + std::to_string(currentTotalCoins_);

    float baseX = ParameterManager::GetInstance()->GetValue("CoinUI", "posX", 40.0f);
    float baseY = ParameterManager::GetInstance()->GetValue("CoinUI", "posY", 35.0f);
    float iconSize = ParameterManager::GetInstance()->GetValue("CoinUI", "iconSize", 44.0f);
    float charW = ParameterManager::GetInstance()->GetValue("CoinUI", "charWidth", 24.0f);
    float charH = ParameterManager::GetInstance()->GetValue("CoinUI", "charHeight", 32.0f);
    float spacing = ParameterManager::GetInstance()->GetValue("CoinUI", "spacing", 2.0f);

    float scaleAnim = 1.0f;
    if (coinPopAnimTimer_ > 0.0f) {
        float progress = coinPopAnimTimer_ / 0.35f;
        scaleAnim = 1.0f + 0.4f * std::sin(progress * 3.14159265f);
    }

    float currentIconSize = iconSize * scaleAnim;
    float iconOffset = (currentIconSize - iconSize) * 0.5f;
    coinSprite_->SetPosition({ baseX - iconOffset, baseY - iconOffset });
    coinSprite_->SetSize({ currentIconSize, currentIconSize });
    coinSprite_->Update();
    coinSprite_->Draw();

    float currentX = baseX + iconSize + 12.0f;
    float textY = baseY + (iconSize - charH) * 0.5f;

    size_t spriteIdx = 0;
    for (char c : text) {
        if (spriteIdx >= numberSprites_.size()) break;
        int charIndex = -1;
        if (c >= '0' && c <= '9') {
            charIndex = c - '0';
        } else if (c == '/') {
            charIndex = 10;
        }

        if (charIndex >= 0) {
            auto& numSprite = numberSprites_[spriteIdx];
            numSprite->SetTextureRect(charIndex * 48.0f, 0.0f, 48.0f, 64.0f);
            numSprite->SetPosition({ currentX, textY });
            numSprite->SetSize({ charW, charH });
            numSprite->Update();
            numSprite->Draw();
            spriteIdx++;
        }
        currentX += charW + spacing;
    }
}
