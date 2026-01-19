#pragma once
#include "ModelCommon.h" // Commonをインクルード
#include "Utility/Utilityfunctions.h"
#include <string>
#include <vector>
#include <wrl.h>

class Model {
public:
    // コンストラクタ・デストラクタ
    Model() = default;
    ~Model();

    // ★初期化関数（OBJファイル読み込み）
    // 従来の CreateFromObj の代わり
    void Initialize(ModelCommon *modelCommon, const std::string &directoryPath, const std::string &filename);

    // ★初期化関数（球体生成）
    // 従来の CreateSphere の代わり
    void InitializeSphere(ModelCommon *modelCommon);

    // 描画
    void Draw(const Matrix4x4 &viewProjectionMatrix);
    // ゲッター
    ModelData GetModelData() const { return modelData_; }


    // --- セッター ---
    void SetTranslation(const Vector3 &position) { transform_.translate = position; }
    void SetRotation(const Vector3 &rotation) { transform_.rotate = rotation; }
    void SetScale(const Vector3 &scale) { transform_.scale = scale; }
    // テクスチャを後から変えたい場合用
    void SetTextureHandle(D3D12_GPU_DESCRIPTOR_HANDLE handle) { textureHandle_ = handle; }

    // --- ゲッター ---
    const Vector3 &GetTranslation() const { return transform_.translate; }
    const Vector3 &GetRotation() const { return transform_.rotate; }
    const Vector3 &GetScale() const { return transform_.scale; }
    const Transform &GetTransform() const { return transform_; }

private:
    // バッファ生成（内部で使うヘルパー関数）
    void CreateBuffers();

private:
    // ModelCommonへのポインタを持つ（ここからDeviceやCommandListをもらう）
    ModelCommon *modelCommon_ = nullptr;

    // 自分の座標情報
    Transform transform_ = { {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };

    // 自分のテクスチャ
    D3D12_GPU_DESCRIPTOR_HANDLE textureHandle_{};

    // 個別のデータ
    ModelData modelData_;
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
    Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_;
    D3D12_INDEX_BUFFER_VIEW indexBufferView_{};

    Microsoft::WRL::ComPtr<ID3D12Resource> transformResource_; // 行列用の箱
    TransformMatrix *mappedTransform_ = nullptr;               // 箱の中身へのアクセス権
};