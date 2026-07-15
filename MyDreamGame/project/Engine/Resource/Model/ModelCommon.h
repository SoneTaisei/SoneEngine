#pragma once
#include <d3d12.h>
#include <list>
#include "Core/Utility/Utilityfunctions.h"

class Model;
class ModelCommon;

// CameraForGPU は Structs.h に移動しました

class ModelCommon {
public:
    // 初期化（Deviceを受け取る）
    void Initialize(ID3D12Device *device);

    // 描画前処理（CommandListを受け取る）
    void PreDraw();

    // 登録されている全モデルを描画する
    // モデルはViewProjection行列を必要とするので引数で受け取る
    void DrawAll(const Matrix4x4 &viewProjectionMatrix);

    // リスト管理用
    void AddModel(Model *model);
    void RemoveModel(Model *model);
    void ClearAll() { models_.clear(); }

    // ゲッター
    ID3D12Device *GetDevice() const { return device_; }
    ID3D12GraphicsCommandList *GetCommandList() const { return commandList_; }

    void SetCamera(const Matrix4x4 &cameraMatrix) {
        cameraMatrix_ = cameraMatrix;
    }

    // ライトやカメラのデータを更新するためのゲッター/セッター
    DirectionalLight *GetDirectionalLight() { return mappedDirectionalLight_; }
    PointLight *GetPointLight() { return mappedPointLight_; }
    SpotLight *GetSpotLight() { return mappedSpotLight_; }
    CameraForGPU *GetCamera() { return mappedCamera_; }
    
    D3D12_GPU_VIRTUAL_ADDRESS GetDirectionalLightGPUAddress() const { return directionalLightResource_->GetGPUVirtualAddress(); }
    D3D12_GPU_VIRTUAL_ADDRESS GetPointLightGPUAddress() const { return pointLightResource_->GetGPUVirtualAddress(); }
    D3D12_GPU_VIRTUAL_ADDRESS GetSpotLightGPUAddress() const { return spotLightResource_->GetGPUVirtualAddress(); }

    // 設定ファイルからの読み込み
    void LoadLightingConfig();

private:
    ID3D12Device *device_ = nullptr;
    ID3D12GraphicsCommandList *commandList_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource_;
    DirectionalLight *mappedDirectionalLight_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> pointLightResource_;
    PointLight *mappedPointLight_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> cameraResource_;
    CameraForGPU *mappedCamera_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
    Material *mappedMaterial_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> spotLightResource_;
    SpotLight *mappedSpotLight_ = nullptr;

    // 全モデルのリスト
    std::list<Model *> models_;

    Matrix4x4 cameraMatrix_ = TransformFunctions::MakeIdentity4x4();
};

