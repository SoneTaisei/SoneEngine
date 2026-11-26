#pragma once
#include "Game2D/IGameUI.h"
#include <cstdint>
#include <memory>
class Sprite;
class SpriteCommon;
class ReadyGoUI : public IGameUI {
public:
    ReadyGoUI() = default;
    ~ReadyGoUI() override = default;
    void Initialize(SpriteCommon *spriteCommon) override;
    void Start();
    void Reset() override;
    void Update() override;
    void Draw() override;
    // 演出が終了したか（!isActive_）
    bool IsFinished() const { return !isActive_; }
    // プレイヤーの操作を許可してよいか（inputDelay経過後ならtrue）
    bool CanControl() const;
    // 演出中かどうか
    bool IsActive() const { return isActive_; }

private:
    bool isActive_ = true;
    float timer_ = 0.0f;

    uint32_t readyTextureHandle_ = 0;
    uint32_t goTextureHandle_ = 0;
    uint32_t whiteTextureHandle_ = 0;

    std::unique_ptr<Sprite> readySprite_;
    std::unique_ptr<Sprite> goSprite_;
    std::unique_ptr<Sprite> readyBarBgSprite_;
    std::unique_ptr<Sprite> readyBarFillSprite_;
};
