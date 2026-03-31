#pragma once
#include <d3d12.h>
#include <wrl.h>
#include "Core/Utility/Utilityfunctions.h"

// 前方宣言
class SpriteCommon;

class Sprite {
public:
    // コンストラクタ・デストラクタ
    Sprite();
    ~Sprite();

    // 初期化 (Commonへのポインタを渡すことで紐付ける)
    void Initialize(SpriteCommon *spriteCommon, uint32_t textureIndex);

    // 更新 (アニメーション等あれば)
    void Update();

    // 描画 (Commonから呼ばれる)
    void Draw();

    // --- セッター ---
    void SetPosition(const Vector2 &position) { transform_.translate = { position.x, position.y, 0.0f }; }
    void SetRotation(float rotation) { transform_.rotate.z = rotation; }
    void SetSize(const Vector2 &size) { transform_.scale = { size.x, size.y, 1.0f }; }
    void SetColor(const Vector4 &color) { materialData_->color = color; }

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
    Transform transform_{ {1.0f,1.0f,1.0f}, {0.0f,0.0f,0.0f}, {0.0f,0.0f,0.0f} };

    // テクスチャ情報
    uint32_t textureIndex_ = 0;

    // 切り抜き用パラメータ
    Vector2 texBaseSize_ = { 100.0f, 100.0f }; // 仮初期値
    Vector2 texPos_ = { 0.0f, 0.0f };
    Vector2 texSize_ = { 100.0f, 100.0f };
    bool isCutMode_ = false;
};