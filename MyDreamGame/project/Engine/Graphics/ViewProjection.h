#pragma once
#include "Core/Utility/Utilityfunctions.h" // ViewProjection構造体の定義がある前提
#include <d3d12.h>
#include <wrl.h>

class ViewProjection {
public:
    void Initialize(ID3D12Device *device);
    void UpdateMatrix(const Matrix4x4 &view, const Matrix4x4 &projection);

    // ゲッター
    ID3D12Resource *GetResource() const { return resource_.Get(); }
    D3D12_GPU_VIRTUAL_ADDRESS GetGPUVirtualAddress() const { return resource_->GetGPUVirtualAddress(); }

    // CPU側での計算に行列の実データを使えるようにする
    const Matrix4x4 &GetMatrix() const { return mappedData_->viewProjectionMatrix; }

private:
    Microsoft::WRL::ComPtr<ID3D12Resource> resource_;
    ViewProjectionData *mappedData_ = nullptr; // 転送用ポインタ
};