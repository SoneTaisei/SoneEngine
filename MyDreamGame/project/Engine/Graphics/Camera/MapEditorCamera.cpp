#include "MapEditorCamera.h"
#include "Core/Utility/TransformFunctions.h"
#ifdef USE_IMGUI
#include <imgui.h>
#endif
#include "Graphics/CameraManager.h"

void MapEditorCamera::Initialize(int kClientWidth, int kClientHeight) {
    Camera::Initialize(kClientWidth, kClientHeight);
    
    // マップの中心付近を初期位置とする
    SetTranslation({10.0f, 10.0f, -50.0f});
    SetRotation({0.0f, 0.0f, 0.0f});
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
            Vector3 translation = GetTranslation();
            translation.x -= delta.x / zoom_;
            translation.y += delta.y / zoom_; // Yは上が正なので逆
            SetTranslation(translation);
        }
#endif
    }

    UpdateMatrix();
}

void MapEditorCamera::UpdateMatrix() {
    // 2Dエディタ用のため回転は常に0（正面向き）に固定
    SetRotation({0.0f, 0.0f, 0.0f});
    SetViewMatrix(TransformFunctions::Inverse(TransformFunctions::MakeTranslateMatrix(GetTranslation())));
    
    // Orthographic projection based on zoom
    float halfWidth = (GetClientWidth() / 2.0f) / zoom_;
    float halfHeight = (GetClientHeight() / 2.0f) / zoom_;
        SetProjectionMatrix(TransformFunctions::MakeOrthographicMatrix(-halfWidth, halfHeight, halfWidth, -halfHeight, 0.1f, 1000.0f));

    CameraManager::GetInstance()->SetCameraInfo(GetTranslation(), GetViewMatrix(), GetProjectionMatrix());
}
