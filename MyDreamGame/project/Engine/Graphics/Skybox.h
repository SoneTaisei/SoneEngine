#pragma once
#include "Core/Utility/Structs.h"
#include <d3d12.h>
#include <vector>
#include <wrl.h>

class Skybox {
public:
    // 初期化（Object3Dと同じくDeviceとテクスチャハンドルを受け取る）
    void Initialize(ID3D12Device *device, uint32_t textureHandle);

    // 更新（カメラの位置と、ビュー・プロジェクション行列を受け取る）
    void Update(const Vector3 &cameraPos, const Matrix4x4 &viewMatrix, const Matrix4x4 &projectionMatrix);

    // 描画（ルールは内部でDirectXCommonから取得するため引数はスッキリ！）
    void Draw(ID3D12GraphicsCommandList *commandList);

private:
    uint32_t textureHandle_ = 0;

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