#include "DebugCamera.h"
#include "Core/Utility/TransformFunctions.h"
#include <algorithm>

void DebugCamera::Initialize(int kClientWidth, int kClientHeight) {
    // 親クラスの初期化を呼ぶ
    Camera::Initialize(kClientWidth, kClientHeight);

    // リセット用に現在の値を保存（transform_は親クラスのメンバ変数）
    initialRotation_ = GetRotation();
    initialTranslation_ = GetTranslation();
}

void DebugCamera::Update(bool allowInput) {
#ifdef USE_IMGUI
    if (allowInput) {
        // --- 定数 ---
        const float moveSpeed = 0.02f;
        const float rotateSpeed = 0.005f;
        const float zoomSpeed = 0.4f; // ホイール感度

        ImGuiIO &io = ImGui::GetIO();

        // --- 回転 (右クリック) ---
        if (io.MouseDown[1]) {
            Vector3 rot = GetRotation();
            rot.y += io.MouseDelta.x * rotateSpeed;
            rot.x += io.MouseDelta.y * rotateSpeed;
            rot.x = std::clamp(rot.x, -1.57f, 1.57f);
            SetRotation(rot);
        }

        // --- カメラのベクトル計算 ---
        Matrix4x4 rotationMatrix = TransformFunctions::Multiply(
            TransformFunctions::MakeRoteXMatrix(GetRotation().x),
            TransformFunctions::MakeRoteYMatrix(GetRotation().y));
        Vector3 forward = {rotationMatrix.m[2][0], rotationMatrix.m[2][1], rotationMatrix.m[2][2]};
        Vector3 right = {rotationMatrix.m[0][0], rotationMatrix.m[0][1], rotationMatrix.m[0][2]};
        Vector3 up = {rotationMatrix.m[1][0], rotationMatrix.m[1][1], rotationMatrix.m[1][2]};

        // --- 移動 ---
        // ズーム (ホイール)
        if (io.MouseWheel != 0.0f && !io.KeyCtrl && !io.MouseDown[2]) {
            SetTranslation(TransformFunctions::AddV(GetTranslation(), TransformFunctions::MultiplyV(io.MouseWheel * zoomSpeed, forward)));
        }
        // 平行移動 (中クリック)
        if (io.MouseDown[2]) {
            SetTranslation(TransformFunctions::AddV(GetTranslation(), TransformFunctions::MultiplyV(-io.MouseDelta.x * moveSpeed, right)));
            SetTranslation(TransformFunctions::AddV(GetTranslation(), TransformFunctions::MultiplyV(io.MouseDelta.y * moveSpeed, up)));
        }
    }
#endif // USE_IMGUI

    // ★重要: 行列計算は常に行う
    Camera::UpdateMatrix();
}