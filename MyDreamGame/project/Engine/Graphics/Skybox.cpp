#include "Skybox.h"
#include "Renderer/DirectXCommon/DirectXCommon.h"
#include "Core/Utility/TransformFunctions.h"
#include "Core/Utility/UtilityFunctions.h"
#include "Graphics/TextureManager.h"
#include "CameraManager.h"

void Skybox::Initialize(ID3D12Device *device, uint32_t textureHandle) {
    textureHandle_ = textureHandle;

    // 1. 頂点データとインデックスデータの生成
    std::vector<SkyboxVertexData> vertices;
    std::vector<uint32_t> indices;
    CreateBoxMesh(vertices, indices); // UtilityFunctions等に実装済みの箱生成関数
    indexCount_ = static_cast<uint32_t>(indices.size());

    // 2. 頂点バッファの作成とデータ転送
    vertexBuffer_ = CreateBufferResource(device, sizeof(SkyboxVertexData) * vertices.size());
    vbView_.BufferLocation = vertexBuffer_->GetGPUVirtualAddress();
    vbView_.SizeInBytes = static_cast<UINT>(sizeof(SkyboxVertexData) * vertices.size());
    vbView_.StrideInBytes = sizeof(SkyboxVertexData);

    SkyboxVertexData *vertexData = nullptr;
    vertexBuffer_->Map(0, nullptr, reinterpret_cast<void **>(&vertexData));
    std::memcpy(vertexData, vertices.data(), sizeof(SkyboxVertexData) * vertices.size());
    vertexBuffer_->Unmap(0, nullptr);

    // 3. インデックスバッファの作成とデータ転送
    indexBuffer_ = CreateBufferResource(device, sizeof(uint32_t) * indices.size());
    ibView_.BufferLocation = indexBuffer_->GetGPUVirtualAddress();
    ibView_.SizeInBytes = static_cast<UINT>(sizeof(uint32_t) * indices.size());
    ibView_.Format = DXGI_FORMAT_R32_UINT;

    uint32_t *indexData = nullptr;
    indexBuffer_->Map(0, nullptr, reinterpret_cast<void **>(&indexData));
    std::memcpy(indexData, indices.data(), sizeof(uint32_t) * indices.size());
    indexBuffer_->Unmap(0, nullptr);

    // 4. 定数バッファの作成 (256バイトアライメントを適用！)
    transformBuffer_ = CreateBufferResource(device, (sizeof(TransformationMatrix) + 255) & ~255u);
    transformBuffer_->Map(0, nullptr, reinterpret_cast<void **>(&mappedTransform_));
    // 初期値として単位行列を入れておく
    mappedTransform_->WVP = TransformFunctions::MakeIdentity4x4();
    mappedTransform_->World = TransformFunctions::MakeIdentity4x4();

    materialBuffer_ = CreateBufferResource(device, (sizeof(Material) + 255) & ~255u);
    materialBuffer_->Map(0, nullptr, reinterpret_cast<void **>(&mappedMaterial_));
    // 色は指定された値 (RGB: 150) に設定
    mappedMaterial_->color = {150.0f / 255.0f, 150.0f / 255.0f, 150.0f / 255.0f, 1.0f};
}

#ifdef USE_IMGUI
#include <imgui.h>
#endif
#include <algorithm>
#include <cmath>

void Skybox::Update() {
    if (!mappedTransform_) {
        return;
    }

    CameraManager *cameraMgr = CameraManager::GetInstance();
    Matrix4x4 view = cameraMgr->GetViewMatrix();
    const Matrix4x4 &proj = cameraMgr->GetProjectionMatrix();
    Vector3 camPos = cameraMgr->GetCameraPos();

    // 1. 射影行列の計算（カメラのアップ・ズームに適応）
    const float nearClip = 0.1f;
    const float farClip = 1000.0f;
    Matrix4x4 projection{};

    // 透視投影（Perspective）判定: proj.m[2][3] == 1.0f かつ proj.m[3][3] == 0.0f
    bool isPerspective = (proj.m[2][3] == 1.0f && proj.m[3][3] == 0.0f);

    if (isPerspective) {
        // 透視投影カメラ（DebugCameraや3Dモード）:
        // カメラの画角・アスペクト比・アップ率（P00, P11）をそのまま適用
        projection.m[0][0] = proj.m[0][0];
        projection.m[1][1] = proj.m[1][1];
        projection.m[2][2] = farClip / (farClip - nearClip);
        projection.m[2][3] = 1.0f;
        projection.m[3][2] = (-nearClip * farClip) / (farClip - nearClip);
        projection.m[3][3] = 0.0f;
    } else {
        // 直交投影カメラ（GameCameraの2Dモード等）:
        // 正射影行列の P11 から表示高さ orthoHeight を逆算
        float orthoHeight = (std::abs(proj.m[1][1]) > 0.0001f) ? (2.0f / proj.m[1][1]) : 11.25f;
        float aspect = (std::abs(proj.m[0][0]) > 0.0001f) ? (proj.m[1][1] / proj.m[0][0]) : (1280.0f / 720.0f);

        // 基準縦幅 11.25f に対するカメラのズームスケール S を算出
        float currentScale = 11.25f / (std::max)(0.001f, orthoHeight);

        // カメラのアップ（ズームイン）に合わせて Skybox の実効 FOV を縮小（アップ）
        float effectiveFov = 2.0f * std::atan(std::tan(baseFov_ * 0.5f) / (std::max)(0.01f, currentScale));
        effectiveFov = (std::clamp)(effectiveFov, 0.05f, 2.5f);

        projection = TransformFunctions::MakePerspectiveFovMatrix(effectiveFov, aspect, nearClip, farClip);
    }

    // 2. ビュー行列の平行移動成分をゼロにして原点中心にする
    view.m[3][0] = 0.0f;
    view.m[3][1] = 0.0f;
    view.m[3][2] = 0.0f;
    view.m[3][3] = 1.0f;

    // 3. 見渡し・視差回転の合成
    Vector3 totalRot = rotationOffset_;
    if (!isPerspective && enableParallax_) {
        // 2Dモード時、カメラの移動（camPos.x, camPos.y）に連動して天球を回転させて見渡せるようにする
        totalRot.y += -camPos.x * parallaxScale_.x;
        totalRot.x += camPos.y * parallaxScale_.y;
    }

    Matrix4x4 rotX = TransformFunctions::MakeRoteXMatrix(totalRot.x);
    Matrix4x4 rotY = TransformFunctions::MakeRoteYMatrix(totalRot.y);
    Matrix4x4 rotZ = TransformFunctions::MakeRoteZMatrix(totalRot.z);
    Matrix4x4 extraRot = TransformFunctions::Multiply(TransformFunctions::Multiply(rotX, rotY), rotZ);

    // ビュー行列に見渡し回転を合成
    Matrix4x4 finalView = TransformFunctions::Multiply(extraRot, view);

    // 4. WVP行列の合成: WVP = World(Identity) * View * Projection
    Matrix4x4 worldMatrix = TransformFunctions::MakeIdentity4x4();
    mappedTransform_->WVP = TransformFunctions::Multiply(worldMatrix, TransformFunctions::Multiply(finalView, projection));
    mappedTransform_->World = worldMatrix;
}

#include <Windows.h>

void Skybox::Draw() {
    // 描画直前の最新のカメラ情報（回転・アップ）を反映
    Update();

    auto commandList = DirectXCommon::GetInstance()->GetCommandList();
    // 1. DirectXCommonから専用のルール（PSO・RootSignature）を取得してセット
    DirectXCommon *dxCommon = DirectXCommon::GetInstance();
    commandList->SetGraphicsRootSignature(dxCommon->GetSkyboxRootSignature());
    commandList->SetPipelineState(dxCommon->GetSkyboxPipelineState());

    // 2. 頂点とインデックスをセット
    commandList->IASetVertexBuffers(0, 1, &vbView_);
    commandList->IASetIndexBuffer(&ibView_);
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // 3. 定数バッファをセット（シェーダーの register(b0), register(b1) に対応）
    if (!transformBuffer_ || !materialBuffer_) {
        return;
    }

    commandList->SetGraphicsRootConstantBufferView(0, transformBuffer_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(1, materialBuffer_->GetGPUVirtualAddress());

    // 4. テクスチャ(CubeMap)のSRVをセット（t0 に対応）
    D3D12_GPU_DESCRIPTOR_HANDLE srvHandle = TextureManager::GetInstance()->GetSrvHandleGPU(textureHandle_);
    if (srvHandle.ptr != 0) {
        commandList->SetGraphicsRootDescriptorTable(2, srvHandle);
    }

    // 5. 描画！（インデックス描画）
    commandList->DrawIndexedInstanced(indexCount_, 1, 0, 0, 0);

    // ★ Skybox描画後に標準の RootSignature に戻す
    commandList->SetGraphicsRootSignature(dxCommon->GetRootSignature());
}

#ifdef USE_IMGUI
void Skybox::DrawImGui() {
    if (ImGui::TreeNode("Skybox (天球設定)")) {
        ImGui::DragFloat3("回転オフセット (rad)", &rotationOffset_.x, 0.01f);
        ImGui::Checkbox("2D移動連動視差を有効化", &enableParallax_);
        if (enableParallax_) {
            ImGui::DragFloat2("視差スケール (X, Y)", &parallaxScale_.x, 0.001f, 0.0f, 0.1f, "%.4f");
        }
        ImGui::DragFloat("基準FOV (rad)", &baseFov_, 0.01f, 0.1f, 2.0f);
        if (mappedMaterial_) {
            ImGui::ColorEdit4("Skybox カラー", &mappedMaterial_->color.x);
        }
        ImGui::TreePop();
    }
}
#endif