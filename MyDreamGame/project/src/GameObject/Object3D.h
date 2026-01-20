#pragma once
#include "Utility/Structs.h" // 必要な構造体のインクルード
#include "Utility/UtilityFunctions.h"

struct CameraForGPU {
    Vector3 worldPosition;
};

class Object3D {
public:
    /// <summary>
    /// 初期化
    /// </summary>
    void Initialize(
        ID3D12Device *device, ModelData *modelData,
        const std::wstring &textureFilePath = L"");

    /// <summary>
    /// 毎フレームの更新
    /// </summary>
    void Update(const Matrix4x4 &viewMatrix, const Matrix4x4 &projectionMatrix, const Vector3 &cameraPos);

    /// <summary>
    /// 描画
    /// </summary>
    void Draw(ID3D12GraphicsCommandList *commandList, ID3D12DescriptorHeap *srvDescriptorHeap);

    /// <summary>
    /// ImGuiでのUI表示
    /// </summary>
    void DisplayImGui(const std::string &label);

    void SetTextureHandle(uint32_t textureSrvHandle) { textureSrvHandle_ = textureSrvHandle; }

    void SetTransform(Transform transform) { transform_ = transform; }

    void SetLightingType(uint32_t lightingType) { material_.lightingType = lightingType; }

private:
    // 定数バッファ
    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
    Material *mappedMaterial_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> lightResource_;
    DirectionalLight *mappedLight_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> transformResource_;
    TransformMatrix *mappedTransform_ = nullptr;

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

    // テクスチャ関連
    Microsoft::WRL::ComPtr<ID3D12Resource> textureResource_;
    uint32_t textureSrvHandle_ = 0; // SRVヒープ上のインデックス

    Microsoft::WRL::ComPtr<ID3D12Resource> cameraResource_;
    CameraForGPU *mappedCamera_ = nullptr;
};

