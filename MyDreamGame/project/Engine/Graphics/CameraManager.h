#pragma once
#include "Core/Utility/Structs.h"
#include <d3d12.h>
#include <wrl.h>

class CameraManager {
public:
    static CameraManager *GetInstance() {
        static CameraManager instance;
        return &instance;
    }

    void Initialize(ID3D12Device* device);

    // カメラ情報の更新（カメラクラスから呼ばれる）
    void SetCameraInfo(const Vector3 &pos, const Matrix4x4 &view, const Matrix4x4 &projection) {
        cameraPos_ = pos;
        viewMatrix_ = view;
        projectionMatrix_ = projection;
        UpdateBuffer();
    }

    void SetCullingCameraInfo(const Matrix4x4& view, const Matrix4x4& projection) {
        cullingViewMatrix_ = view;
        cullingProjectionMatrix_ = projection;
        useCullingCamera_ = true;
    }

    void ClearCullingCameraInfo() {
        useCullingCamera_ = false;
    }

    // ゲッター
    const Vector3 &GetCameraPos() const { return cameraPos_; }
    const Matrix4x4 &GetViewMatrix() const { return viewMatrix_; }
    const Matrix4x4 &GetProjectionMatrix() const { return projectionMatrix_; }
    
    // カリング判定用ゲッター
    const Matrix4x4 &GetCullingViewMatrix() const { return useCullingCamera_ ? cullingViewMatrix_ : viewMatrix_; }
    const Matrix4x4 &GetCullingProjectionMatrix() const { return useCullingCamera_ ? cullingProjectionMatrix_ : projectionMatrix_; }

    D3D12_GPU_VIRTUAL_ADDRESS GetCameraGPUAddress() const { 
        if(!cameraResource_) return 0;
        return cameraResource_->GetGPUVirtualAddress(); 
    }

private:
    CameraManager() = default;
    void UpdateBuffer();

    Vector3 cameraPos_{};
    Matrix4x4 viewMatrix_{};
    Matrix4x4 projectionMatrix_{};

    Matrix4x4 cullingViewMatrix_{};
    Matrix4x4 cullingProjectionMatrix_{};
    bool useCullingCamera_ = false;

    Microsoft::WRL::ComPtr<ID3D12Resource> cameraResource_;
    CameraForGPU* mappedCamera_ = nullptr;
};