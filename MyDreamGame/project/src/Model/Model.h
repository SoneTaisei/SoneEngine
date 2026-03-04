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
    void Draw();

    void SetTextureHandle(D3D12_GPU_DESCRIPTOR_HANDLE handle) { textureHandle_ = handle; }
    // ゲッター
    ModelData GetModelData() const { return modelData_; }


private:
    // バッファ生成（内部で使うヘルパー関数）
    void CreateBuffers();

private:
    // ModelCommonへのポインタを持つ（ここからDeviceやCommandListをもらう）
    ModelCommon *modelCommon_ = nullptr;

    // 自分のテクスチャ
    D3D12_GPU_DESCRIPTOR_HANDLE textureHandle_{};

    // 個別のデータ
    ModelData modelData_;
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
    Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_;
    D3D12_INDEX_BUFFER_VIEW indexBufferView_{};
};