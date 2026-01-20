#include "Object3D.h"
#include <DirectXMath.h>

void Object3D::Initialize(ID3D12Device *device, ModelData *modelData, const std::wstring &textureFilePath) {
    // メンバ変数の初期値を設定
    transform_ = {{1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}};

    // マテリアル初期化
    material_ = {
        {1.0f, 1.0f, 1.0f, 1.0f},
        1, // lightingType
        1, // enableBlinnPhong
        {0.0f, 0.0f},
        TransformFunctions::MakeIdentity4x4(),
        50.0f // shininess
    };

    // 平行光源初期化
    light_ = {{1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, -1.0f, 0.0f}, 1.0f};

    // ポイントライト初期化 (★追加)
    pointLight_ = {{1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 2.0f, 0.0f}, 1.0f};

    // --- リソース作成 ---

    // 1. マテリアル
    materialResource_ = CreateBufferResource(device, (sizeof(Material) + 255) & ~255u);
    materialResource_->Map(0, nullptr, reinterpret_cast<void **>(&mappedMaterial_));

    // 2. 平行光源
    lightResource_ = CreateBufferResource(device, (sizeof(DirectionalLight) + 255) & ~255u);
    lightResource_->Map(0, nullptr, reinterpret_cast<void **>(&mappedLight_));

    // 3. 座標変換
    transformResource_ = CreateBufferResource(device, (sizeof(TransformMatrix) + 255) & ~255u);
    transformResource_->Map(0, nullptr, reinterpret_cast<void **>(&mappedTransform_));

    // 4. ポイントライト (★ここが抜けていました！)
    pointLightResource_ = CreateBufferResource(device, (sizeof(PointLight) + 255) & ~255u);
    pointLightResource_->Map(0, nullptr, reinterpret_cast<void **>(&mappedPointLight_));
    // 初期値をコピー
    *mappedPointLight_ = pointLight_;

    // 5. カメラ (★変数を cameraResource_ に統一)
    cameraResource_ = CreateBufferResource(device, (sizeof(CameraForGPU) + 255) & ~255u);
    cameraResource_->Map(0, nullptr, reinterpret_cast<void **>(&mappedCamera_));
    mappedCamera_->worldPosition = {0.0f, 0.0f, -5.0f}; // 適当な初期値

    // 頂点バッファ等の作成処理 (省略されている部分はそのまま)
    // ...
}

void Object3D::Update(const Matrix4x4 &viewMatrix, const Matrix4x4 &projectionMatrix, const Vector3 &cameraPos) {
    // データ転送
    *mappedMaterial_ = material_;
    *mappedLight_ = light_;

    // ポイントライトも更新 (ImGuiなどで動かすかもしれないので)
    *mappedPointLight_ = pointLight_;

    // 行列計算
    Matrix4x4 worldMatrix = TransformFunctions::MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);
    Matrix4x4 wvpMatrix = worldMatrix * viewMatrix * projectionMatrix;

    // 逆転置行列
    DirectX::XMMATRIX worldX = DirectX::XMLoadFloat4x4(reinterpret_cast<const DirectX::XMFLOAT4X4 *>(&worldMatrix));
    DirectX::XMMATRIX worldInv = DirectX::XMMatrixInverse(nullptr, worldX);
    DirectX::XMMATRIX worldInvTrans = worldInv; // 転置はhlsl側でするか、ここでするかによりますがコード通り

    // 転送
    mappedTransform_->World = worldMatrix;
    mappedTransform_->WVP = wvpMatrix;
    DirectX::XMStoreFloat4x4(reinterpret_cast<DirectX::XMFLOAT4X4 *>(&mappedTransform_->WorldInverseTranspose), worldInvTrans);

    // カメラ位置更新 (★変数を mappedCamera_ に統一)
    mappedCamera_->worldPosition = cameraPos;
}

void Object3D::Draw(ID3D12GraphicsCommandList *commandList, ID3D12DescriptorHeap *srvDescriptorHeap) {
    // 0: マテリアル
    commandList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());

    // 1: 座標変換
    commandList->SetGraphicsRootConstantBufferView(1, transformResource_->GetGPUVirtualAddress());

    // 2: テクスチャ (DescriptorTable)
    // ※セット済み前提なら省略可、そうでなければここでセット

    // 3: ポイントライト (b3)
    // ★ここが大事！作成した pointLightResource_ をセット
    commandList->SetGraphicsRootConstantBufferView(3, pointLightResource_->GetGPUVirtualAddress());

    // 4: 平行光源 (b1)
    commandList->SetGraphicsRootConstantBufferView(4, lightResource_->GetGPUVirtualAddress());

    // 5: カメラ (b2)
    // ★ここも大事！作成した cameraResource_ をセット (cameraBuffer_は削除)
    commandList->SetGraphicsRootConstantBufferView(5, cameraResource_->GetGPUVirtualAddress());

    // 描画
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
    commandList->IASetIndexBuffer(&indexBufferView_);
    commandList->DrawIndexedInstanced(indexCount_, 1, 0, 0, 0);
}