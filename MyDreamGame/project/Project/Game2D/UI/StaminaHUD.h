#pragma once
#include "Game2D/IGameUI.h"
#include "Core/Utility/Structs.h"
#include <cstdint>
#include <memory>

class Sprite;
class SpriteCommon;
class Player2D;

/// <summary>
/// 壁登り時のプレイヤースタミナ（体力）バーUI
/// プレイヤーの横（壁と反対側のスペース）に追従して表示する
/// </summary>
class StaminaHUD : public IGameUI {
public:
    StaminaHUD() = default;
    ~StaminaHUD() override = default;

    void Initialize(SpriteCommon* spriteCommon) override;
    void Update() override;
    void Draw() override;
    void Reset() override;

    void SetPlayer(const Player2D* player) { player_ = player; }

private:
    bool WorldToSpritePos(const Vector3& worldPos, Vector2& outSpritePos) const;
    Vector4 ComputeFillColor(float ratio, bool isExhausted) const;

    SpriteCommon* spriteCommon_ = nullptr;
    const Player2D* player_ = nullptr;

    uint32_t whiteTextureHandle_ = 0;
    std::unique_ptr<Sprite> bgBorderSprite_;
    std::unique_ptr<Sprite> bgTrackSprite_;
    std::unique_ptr<Sprite> fillSprite_;

    float alpha_ = 0.0f;
    float currentSide_ = -1.0f; // -1: プレイヤー左側, +1: プレイヤー右側
    float blinkTimer_ = 0.0f;
    float displayRatio_ = 1.0f;
};
