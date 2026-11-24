#pragma once
#include <cstdint>
#include <memory>
#include <vector>
#include "Game2D/IGameUI.h"

class Sprite;
class SpriteCommon;

class CoinHUD:public IGameUI {
public:
    CoinHUD() = default;
    ~CoinHUD() = default;

    void Initialize(SpriteCommon *spriteCommon) override;
    void Update() override;
    void Draw() override;
    void Reset() override;

    // コインの枚数取得
    void SetCoinCounts(int collectedCoins, int totalCoins);

private:
    uint32_t coinTextureHandle_ = 0;
    std::unique_ptr<Sprite> coinSprite_;
    uint32_t numbersTextureHandle_ = 0;
    std::vector<std::unique_ptr<Sprite>> numberSprites_;

    float coinPopAnimTimer_ = 0.0f;
    int lastCollectedCoins_ = 0;
    int currentCollectedCoins_ = 0;
    int currentTotalCoins_ = 0;
};
