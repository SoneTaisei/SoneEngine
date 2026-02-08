#include "DebugCamera.h"
#include "Utility/TransformFunctions.h"
#include <algorithm>

void DebugCamera::Initialize(int kClientWidth, int kClientHeight) {
    // 親クラスの初期化を呼ぶ
    Camera::Initialize(kClientWidth, kClientHeight);

    // リセット用に現在の値を保存（transform_は親クラスのメンバ変数）
    initialRotation_ = transform_.rotate;
    initialTranslation_ = transform_.translate;
}

void DebugCamera::Update() {
#ifdef USE_IMGUI


    // --- 定数 ---
    const float moveSpeed = 0.02f;
    const float rotateSpeed = 0.005f;
    const float zoomSpeed = 0.4f; // ホイール感度

    ImGuiIO &io = ImGui::GetIO();
    if(io.WantCaptureMouse) { return; }

    // --- 回転 ---
    if(io.MouseDown[1]) { // 右クリック
        transform_.rotate.y += io.MouseDelta.x * rotateSpeed;
        transform_.rotate.x += io.MouseDelta.y * rotateSpeed;
        transform_.rotate.x = std::clamp(transform_.rotate.x, -1.57f, 1.57f);
    }

    // --- カメラのベクトル計算 ---
    // 親クラスの変数 transform_.rotate を使用
    Matrix4x4 rotationMatrix = TransformFunctions::Multiply(
        TransformFunctions::MakeRoteXMatrix(transform_.rotate.x),
        TransformFunctions::MakeRoteYMatrix(transform_.rotate.y)
    );
    Vector3 forward = { rotationMatrix.m[2][0], rotationMatrix.m[2][1], rotationMatrix.m[2][2] };
    Vector3 right = { rotationMatrix.m[0][0], rotationMatrix.m[0][1], rotationMatrix.m[0][2] };
    Vector3 up = { rotationMatrix.m[1][0], rotationMatrix.m[1][1], rotationMatrix.m[1][2] };

    // --- 移動 ---
    // ズーム
    if(io.MouseWheel != 0.0f) {
        transform_.translate = TransformFunctions::AddV(transform_.translate, TransformFunctions::MultiplyV(io.MouseWheel * zoomSpeed, forward));
    }
    // 平行移動
    if(io.MouseDown[2]) {
        transform_.translate = TransformFunctions::AddV(transform_.translate, TransformFunctions::MultiplyV(-io.MouseDelta.x * moveSpeed, right));
        transform_.translate = TransformFunctions::AddV(transform_.translate, TransformFunctions::MultiplyV(io.MouseDelta.y * moveSpeed, up));
    }
#endif // USE_IMGUI

    // ★重要: 最後に親クラスの行列計算関数を呼ぶ！
    // これで viewMatrix_ と projectionMatrix_ が更新される
    Camera::UpdateMatrix();
}