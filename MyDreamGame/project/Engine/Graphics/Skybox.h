#pragma once
#include "Core/Utility/Structs.h"
#include <d3d12.h>
#include <wrl.h>

class Skybox {
public:
    // 初期化（Object3Dと同じくDeviceとテクスチャハンドルを受け取る）
    void Initialize(ID3D12Device *device, uint32_t textureHandle);

    // 更新（カメラの位置と、ビュー・プロジェクション行列を受け取る）
    void Update();

    // 描画（ルールは内部でDirectXCommonから取得するため引数はスッキリ！）
    void Draw();

    // 色（明るさなど）を設定する
    void SetColor(const Vector4& color) { if(mappedMaterial_) mappedMaterial_->color = color; }

    // カメラのアップ・回転・視差関連の設定
    void SetRotationOffset(const Vector3& rot) { rotationOffset_ = rot; }
    const Vector3& GetRotationOffset() const { return rotationOffset_; }

    void SetParallaxScale(const Vector2& scale) { parallaxScale_ = scale; }
    const Vector2& GetParallaxScale() const { return parallaxScale_; }

    void SetBaseFov(float fov) { baseFov_ = fov; }
    float GetBaseFov() const { return baseFov_; }

    void SetEnableParallax(bool enable) { enableParallax_ = enable; }
    bool IsParallaxEnabled() const { return enableParallax_; }

#ifdef USE_IMGUI
    void DrawImGui();
#endif

private:
    uint32_t textureHandle_ = 0;

    // 見渡し・視差・画角設定
    Vector3 rotationOffset_ = { 0.0f, 0.0f, 0.0f }; // 追加の回転（見渡し用）
    Vector2 parallaxScale_ = { 0.015f, 0.015f };    // 2D移動時の視差回転スケール
    float baseFov_ = 0.45f;                         // 基準視野角
    bool enableParallax_ = true;                    // 2D時の移動連動視差を有効にするか

    // バッファリソース
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer_;
    Microsoft::WRL::ComPtr<ID3D12Resource> indexBuffer_;
    Microsoft::WRL::ComPtr<ID3D12Resource> transformBuffer_;
    Microsoft::WRL::ComPtr<ID3D12Resource> materialBuffer_;

    // ビュー
    D3D12_VERTEX_BUFFER_VIEW vbView_{};
    D3D12_INDEX_BUFFER_VIEW ibView_{};

    // シェーダーに送るデータ構造体
    struct TransformationMatrix {
        Matrix4x4 WVP;
        Matrix4x4 World;
    };
    struct Material {
        Vector4 color;
    };

    TransformationMatrix *mappedTransform_ = nullptr;
    Material *mappedMaterial_ = nullptr;

    // インデックスの数
    uint32_t indexCount_ = 0;
};