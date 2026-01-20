#pragma once
#include "Utility/Structs.h"
#include "Utility/UtilityFunctions.h"

// ※CameraForGPU はここにあってもいいですが、他のファイルでも使うなら共通ヘッダー推奨です
struct CameraForGPU {
    Vector3 worldPosition;
};

class Object3D {
public:
    void Initialize(ID3D12Device *device, ModelData *modelData, const std::wstring &textureFilePath = L"");
    void Update(const Matrix4x4 &viewMatrix, const Matrix4x4 &projectionMatrix, const Vector3 &cameraPos);
    void Draw(ID3D12GraphicsCommandList *commandList, ID3D12DescriptorHeap *srvDescriptorHeap);
    void DisplayImGui(const std::string &label);

private:
    // マテリアル
    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
    Material *mappedMaterial_ = nullptr;

    // 平行光源 (Directional Light)
    Microsoft::WRL::ComPtr<ID3D12Resource> lightResource_;
    DirectionalLight *mappedLight_ = nullptr;

    // 座標変換 (World, WVP)
    Microsoft::WRL::ComPtr<ID3D12Resource> transformResource_;
    TransformMatrix *mappedTransform_ = nullptr;

    // ポイントライト (Point Light)
    Microsoft::WRL::ComPtr<ID3D12Resource> pointLightResource_;
    PointLight *mappedPointLight_ = nullptr; // 名前を統一感あるものに変更

    // カメラ (Camera)
    Microsoft::WRL::ComPtr<ID3D12Resource> cameraResource_; // cameraBuffer_ と重複していたのでこれに統一
    CameraForGPU *mappedCamera_ = nullptr;                  // cameraData_ と重複していたのでこれに統一

    // 頂点・インデックスバッファ
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer_;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
    Microsoft::WRL::ComPtr<ID3D12Resource> indexBuffer_;
    D3D12_INDEX_BUFFER_VIEW indexBufferView_{};
    uint32_t indexCount_ = 0;

    // CPU側データ
    Transform transform_;
    Material material_;
    DirectionalLight light_;
    PointLight pointLight_; // CPU側でも値を保持しておくと便利

    // テクスチャ関連
    Microsoft::WRL::ComPtr<ID3D12Resource> textureResource_;
    uint32_t textureSrvHandle_ = 0;
};