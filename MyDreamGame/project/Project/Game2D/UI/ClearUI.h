#pragma once
#include <cstdint>
#include <memory>

class Sprite;
class SpriteCommon;
class GameCamera;

class ClearUI {
public:
    ClearUI() = default;
    ~ClearUI() = default;

    void Initialize(SpriteCommon* spriteCommon);
    void Start(float initialCameraScale);
    void Reset(GameCamera* camera = nullptr);

    // クリア演出の更新
    void Update(float deltaTime, GameCamera* camera);

    // タイトル画面へ戻る入力が押されたか (Space, Pad A, Pad Start)
    bool IsReturnRequested() const;

    // 演出中かどうか
    bool IsActive() const { return isActive_; }

    // 2D描画
    void Draw();

private:
    bool isActive_ = false;
    float timer_ = 0.0f;
    float initialCameraScale_ = 1.0f;

    uint32_t clearTextureHandle_ = 0;
    std::unique_ptr<Sprite> clearSprite_;
};
