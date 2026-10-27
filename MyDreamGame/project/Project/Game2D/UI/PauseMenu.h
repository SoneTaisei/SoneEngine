#pragma once
#include <cstdint>
#include <memory>

class Sprite;
class SpriteCommon;

enum class PauseMenuAction {
    None,
    Resume,             // つづける
    ExitToStageSelect   // セレクトに戻る
};

class PauseMenu {
public:
    PauseMenu() = default;
    ~PauseMenu() = default;

    void Initialize(SpriteCommon* spriteCommon);
    void Reset();
    PauseMenuAction Update(float deltaTime);
    void Draw();

    int GetSelectedIndex() const { return selectedIndex_; }

private:
    int selectedIndex_ = 0; // 0: つづける, 1: セレクトに戻る

    uint32_t whiteTextureHandle_ = 0;
    uint32_t continueTextureHandle_ = 0;
    uint32_t selectTextureHandle_ = 0;

    std::unique_ptr<Sprite> overlaySprite_;
    std::unique_ptr<Sprite> continueSprite_;
    std::unique_ptr<Sprite> selectSprite_;
};
