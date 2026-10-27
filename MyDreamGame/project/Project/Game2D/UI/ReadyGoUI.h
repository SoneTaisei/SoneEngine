#pragma once
#include <cstdint>
#include <memory>

class Sprite;
class SpriteCommon;

class ReadyGoUI {
public:
    ReadyGoUI() = default;
    ~ReadyGoUI() = default;

    void Initialize(SpriteCommon* spriteCommon);
    void Start();
    void Reset();

    // 更新処理。演出が終了したら true を返す
    bool Update(float deltaTime);

    // プレイヤーの操作を許可してよいか（inputDelay経過後ならtrue）
    bool CanControl() const;

    // 演出中かどうか
    bool IsActive() const { return isActive_; }

    // 2D描画
    void Draw();

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
