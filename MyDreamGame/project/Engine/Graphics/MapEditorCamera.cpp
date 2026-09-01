#include "MapEditorCamera.h"
#include "Core/Utility/TransformFunctions.h"
#include "Input/KeyboardInput.h"
#ifdef USE_IMGUI
#include <imgui.h>
#endif
#include <algorithm>
#include "Graphics/CameraManager.h"

void MapEditorCamera::Initialize(int kClientWidth, int kClientHeight) {
    Camera::Initialize(kClientWidth, kClientHeight);
    
    // マップの中心付近を初期位置とする
    transform_.translate = { 10.0f, 10.0f, -50.0f };
    transform_.rotate = { 0.0f, 0.0f, 0.0f }; // 正面を向く
    zoom_ = 32.0f;
    
    UpdateMatrix();
}

void MapEditorCamera::Update(bool allowInput) {
    if (allowInput) {
#ifdef USE_IMGUI
        ImGuiIO& io = ImGui::GetIO();
        
        // ズーム（マウスホイール）
        if (io.MouseWheel != 0.0f && !io.KeyCtrl && !ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
            float oldZoom = zoom_;
            zoom_ += io.MouseWheel * 4.0f;
            if (zoom_ < 8.0f) zoom_ = 8.0f;
            if (zoom_ > 128.0f) zoom_ = 128.0f;
            
            // ズームした分だけワールド座標の表示範囲が変わるので、必要に応じて補正可能ですが
            // 今回はシンプルにスケール変更のみとします
        }
        
        // パン（中ボタンドラッグ または 右ボタンドラッグ）
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle) || ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
            ImVec2 delta = io.MouseDelta;
            // ピクセル移動量をワールド座標の移動量に変換
            // zoom_ は 1ワールド単位あたりのピクセル数を表す想定
            transform_.translate.x -= delta.x / zoom_;
            transform_.translate.y += delta.y / zoom_; // Yは上が正なので逆
        }
#endif
    }

    UpdateMatrix();
}

void MapEditorCamera::UpdateMatrix() {
    // 2Dエディタ用のため回転は常に0（正面向き）に固定
    transform_.rotate = { 0.0f, 0.0f, 0.0f };
    viewMatrix_ = TransformFunctions::Inverse(TransformFunctions::MakeTranslateMatrix(transform_.translate));
    
    // Orthographic projection based on zoom
    float halfWidth = (kClientWidth_ / 2.0f) / zoom_;
    float halfHeight = (kClientHeight_ / 2.0f) / zoom_;
    projectionMatrix_ = TransformFunctions::MakeOrthographicMatrix(-halfWidth, halfHeight, halfWidth, -halfHeight, 0.1f, 1000.0f);

    CameraManager::GetInstance()->SetCameraInfo(transform_.translate, viewMatrix_, projectionMatrix_);
}
