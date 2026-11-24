#pragma once
#include <cstdint>
#include <memory>
#include "Game2D/IGameUI.h"

class Sprite;
class SpriteCommon;
class GameCamera;

class ClearUI:public IGameUI {
public:
    ClearUI() = default;
    ~ClearUI() = default;

    void Initialize(SpriteCommon *spriteCommon) override;
    void Update() override;
    void Draw() override;
    void Reset() override;

    // タイトル画面へ戻る入力が押されたか (Space, Pad A, Pad Start)
    bool IsReturnRequested() const;

    // 演出中かどうか
    bool IsActive() const { return isActive_; }

    // スタート
    void Start(float initialCameraScale); 
    // カメラをセットする
    void SetCamera(GameCamera *camera) { camera_ = camera; }

private:
    bool isActive_ = false;
    float timer_ = 0.0f;
    float initialCameraScale_ = 1.0f;

    uint32_t clearTextureHandle_ = 0;
    std::unique_ptr<Sprite> clearSprite_;

    GameCamera *camera_ = nullptr;
};
