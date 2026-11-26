#pragma once
#include <cstdint>
#include <memory>
#include "Game2D/IGameUI.h"

class Sprite;
class SpriteCommon;

enum class PauseMenuAction {
    None,
    Resume,             // つづける
    ExitToStageSelect   // セレクトに戻る
};

class PauseMenu:public IGameUI {
public:
    PauseMenu() = default;
    ~PauseMenu() = default;

    void Initialize(SpriteCommon* spriteCommon)override;
    void Update()override;
    void Draw()override;
    void Reset()override;

    int GetSelectedIndex() const { return selectedIndex_; }

    PauseMenuAction GetAction() const { return lastAction_; }

private:
    PauseMenuAction lastAction_ = PauseMenuAction::None;

    int selectedIndex_ = 0; // 0: つづける, 1: セレクトに戻る

    uint32_t whiteTextureHandle_ = 0;
    uint32_t continueTextureHandle_ = 0;
    uint32_t selectTextureHandle_ = 0;

    std::unique_ptr<Sprite> overlaySprite_;
    std::unique_ptr<Sprite> continueSprite_;
    std::unique_ptr<Sprite> selectSprite_;
};
