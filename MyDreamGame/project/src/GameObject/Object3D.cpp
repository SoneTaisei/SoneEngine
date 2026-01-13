// 実際のコードでは各関数の詳細な実装が必要です。
#include "Object3D.h"

void Object3D::Initialize(ID3D12Device *device, ModelData *modelData, const std::wstring &textureFilePath) {
    // メンバ変数の初期値を設定
    transform_ = { {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };
    material_ = { {1.0f, 1.0f, 1.0f, 1.0f}, 1, {}, TransformFunctions::MakeIdentity4x4() };
    light_ = { {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, -1.0f, 0.0f}, 1.0f };

    // 頂点バッファやインデックスバッファをmodelDataから作成
    // (main.cppのCreateBufferResourceやMap/memcpyなどの処理をここに移動)

    // 各種定数バッファを作成し、Mapしておく
    materialResource_ = CreateBufferResource(device, (sizeof(Material) + 255) & ~255u);
    materialResource_->Map(0, nullptr, reinterpret_cast<void **>(&mappedMaterial_));

    lightResource_ = CreateBufferResource(device, (sizeof(DirectionalLight) + 255) & ~255u);
    lightResource_->Map(0, nullptr, reinterpret_cast<void **>(&mappedLight_));

    transformResource_ = CreateBufferResource(device, sizeof(TransformMatrix));
    transformResource_->Map(0, nullptr, reinterpret_cast<void **>(&mappedTransform_));

    // テクスチャを読み込み、SRVを作成する (この部分はTextureManagerクラスなどを作るとさらに良くなる)

    cameraResource_ = CreateBufferResource(device, (sizeof(CameraForGPU) + 255) & ~255u);
    cameraResource_->Map(0, nullptr, reinterpret_cast<void **>(&mappedCamera_));
    mappedCamera_->worldPosition = {0.0f, 0.0f, 0.0f};
}

void Object3D::Update(const Matrix4x4 &viewMatrix, const Matrix4x4 &projectionMatrix, const Vector3 &cameraPos) {
    // ImGuiで変更されたCPU側のデータをGPUリソースにコピー
    *mappedMaterial_ = material_;
    *mappedLight_ = light_;

    // 座標変換行列を計算してGPUリソースにコピー
    Matrix4x4 worldMatrix = TransformFunctions::MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);
    mappedTransform_->World = worldMatrix;
    mappedTransform_->WVP = worldMatrix * viewMatrix * projectionMatrix;

    // CPU側のカメラ位置をバッファに書き込む
    mappedCamera_->worldPosition = cameraPos;
}

void Object3D::Draw(ID3D12GraphicsCommandList *commandList, ID3D12DescriptorHeap *srvDescriptorHeap) {
    // このオブジェクト用のリソースをシェーダーに設定
    commandList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(1, transformResource_->GetGPUVirtualAddress());
    // ※重要: RootSignatureの設定で、新しい定数バッファ(b2)を受け取るスロットを追加する必要があります。
    // ここでは仮にインデックス「3」が空いているとして設定します。
    // RootSignatureの構成によってはインデックス番号が変わるので注意してください。
    commandList->SetGraphicsRootConstantBufferView(3, cameraResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(4, lightResource_->GetGPUVirtualAddress());

    // テクスチャを設定
    // ...

    // 頂点・インデックスバッファを設定
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
    commandList->IASetIndexBuffer(&indexBufferView_);

    // 描画コマンド
    commandList->DrawIndexedInstanced(indexCount_, 1, 0, 0, 0);
}

void Object3D::DisplayImGui(const std::string &label) {
    //if(ImGui::TreeNode(label.c_str())) {
    //    // transform_, material_, light_ の各メンバ変数を操作するUIを作成
    //    ImGui::DragFloat3("Translate", &transform_.translate.x, 0.1f);
    //    // ... (以下、必要なUIを追加)

    //    const char *items[] = { "No Lighting", "Lambert", "Half Lambert" };
    //    ImGui::Combo("Lighting", &material_.lightingType, items, IM_ARRAYSIZE(items));
    //    ImGui::ColorEdit3("Light Color", &light_.color.x);
    //    ImGui::DragFloat("Light Intensity", &light_.intensity, 0.01f);

    //    ImGui::TreePop();
    //}
}
