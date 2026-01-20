#pragma once
#include <d3d12.h>
#include <list>
#include "Utility/Utilityfunctions.h"

class Model;

class ModelCommon {
public:
    // 初期化（Deviceを受け取る）
    void Initialize(ID3D12Device *device);

    // 描画前処理（CommandListを受け取る）
    void PreDraw(ID3D12GraphicsCommandList *commandList);

    // 登録されている全モデルを描画する
    // モデルはViewProjection行列を必要とするので引数で受け取る
    void DrawAll(const Matrix4x4 &viewProjectionMatrix);

    // リスト管理用
    void AddModel(Model *model);
    void RemoveModel(Model *model);

    // ゲッター
    ID3D12Device *GetDevice() const { return device_; }
    ID3D12GraphicsCommandList *GetCommandList() const { return commandList_; }

    void SetCamera(const Matrix4x4 &cameraMatrix) {
        cameraMatrix_ = cameraMatrix;
    }


private:
    ID3D12Device *device_ = nullptr;
    ID3D12GraphicsCommandList *commandList_ = nullptr;

    // 全モデルのリスト
    std::list<Model *> models_;

    Matrix4x4 cameraMatrix_ = TransformFunctions::MakeIdentity4x4();
};

