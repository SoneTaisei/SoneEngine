#pragma once
#include <d3d12.h>
#include <wrl.h>
#include "Core/Utility/Utilityfunctions.h"

// 前方宣言
class SpriteCommon;

// アニメーション設定構造体
struct SpriteAnimationConfig {
    int columns = 1;              // 横の分割数（列数）
    int rows = 1;                 // 縦の分割数（行数）
    int totalFrames = 0;          // 総コマ数（0以下の場合は columns * rows で自動計算）
    float fps = 10.0f;            // 1秒あたりのコマ数
    bool isLoop = true;           // ループ再生するか
};

class Sprite {
    friend class Renderer;
public:
    // コンストラクタ・デストラクタ
    Sprite();
    ~Sprite();

    // 初期化 (Commonへのポインタを渡すことで紐付ける)
    void Initialize(SpriteCommon *spriteCommon, uint32_t textureIndex);

    // 更新 (TimeManager::GetDeltaTime() で自動更新)
    void Update();
    // デルタタイム指定での更新
    void Update(float deltaTime);

    // 描画 (Commonから呼ばれる)
    void Draw();

    // --- アニメーション再生（パターンA：グリッド分割） ---
    void SetAnimationGrid(int columns, int rows, float fps = 10.0f, bool isLoop = true, int totalFrames = 0);
    void SetAnimation(const SpriteAnimationConfig &config);
    void PlayAnimation();
    void PauseAnimation();
    void ResumeAnimation();
    void StopAnimation();
    void SetAnimationFrame(int frameIndex);

    bool IsAnimationActive() const { return isAnimationActive_; }
    bool IsAnimationPlaying() const { return isAnimPlaying_; }
    int GetCurrentAnimationFrame() const { return animCurrentFrame_; }
    int GetAnimationTotalFrames() const { return animTotalFrames_; }
    const SpriteAnimationConfig &GetAnimationConfig() const { return animConfig_; }

    // --- セッター ---
    void SetPosition(const Vector2 &position) { transform_.translate = { position.x, position.y, 0.0f }; }
    void SetRotation(float rotation) { transform_.rotate.z = rotation; }
    void SetSize(const Vector2 &size) { transform_.scale = { size.x, size.y, 1.0f }; }
    void SetColor(const Vector4 &color) { materialData_->color = color; }

    // --- ゲッター ---
    Vector2 GetPosition() const { return { transform_.translate.x, transform_.translate.y }; }
    float GetRotation() const { return transform_.rotate.z; }
    Vector2 GetSize() const { return { transform_.scale.x, transform_.scale.y }; }
    const Vector2 &GetTexBaseSize() const { return texBaseSize_; }
    uint32_t GetTextureIndex() const { return textureIndex_; }

    // テクスチャ切り抜き設定
    void SetTextureRect(float x, float y, float w, float h);
    // 切り抜き解除
    void ResetTextureRect();

private:
    // 借りてくる共通部分
    SpriteCommon *spriteCommon_ = nullptr;

    // 個別のリソース
    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
    Material *materialData_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> transformResource_; // 行列を入れるための箱
    TransformMatrix *mappedTransform_ = nullptr;               // 箱の中身へのアクセス権

    // 座標変換用
    EulerTransform transform_{ {1.0f,1.0f,1.0f}, {0.0f,0.0f,0.0f}, {0.0f,0.0f,0.0f} };

    // テクスチャ情報
    uint32_t textureIndex_ = 0;

    // 切り抜き用パラメータ
    Vector2 texBaseSize_ = { 100.0f, 100.0f }; // 仮初期値
    Vector2 texPos_ = { 0.0f, 0.0f };
    Vector2 texSize_ = { 100.0f, 100.0f };
    bool isCutMode_ = false;

    // アニメーション制御用
    SpriteAnimationConfig animConfig_{};
    bool isAnimationActive_ = false;
    bool isAnimPlaying_ = false;
    float animTimer_ = 0.0f;
    int animCurrentFrame_ = 0;
    int animTotalFrames_ = 1;
};