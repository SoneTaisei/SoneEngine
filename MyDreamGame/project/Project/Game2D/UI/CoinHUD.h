#pragma once
#include <cstdint>
#include <memory>
#include <vector>

class Sprite;
class SpriteCommon;

class CoinHUD {
public:
    CoinHUD() = default;
    ~CoinHUD() = default;

    void Initialize(SpriteCommon* spriteCommon);
    void Update(float deltaTime, int collectedCoins, int totalCoins);
    void Draw();

    void Reset();

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
