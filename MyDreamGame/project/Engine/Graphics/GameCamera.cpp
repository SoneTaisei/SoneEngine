#include "GameCamera.h"
#include "CameraManager.h"
#include "Core/Utility/TransformFunctions.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>

void GameCamera::Initialize(int kClientWidth, int kClientHeight) {
    Camera::Initialize(kClientWidth, kClientHeight);
    // 初期位置の設定（例えばプレイヤーの少し後ろ）
    transform_.translate = { 0.0f, 0.0f, -10.0f };
    transform_.rotate = { 0.0f, 0.0f, 0.0f };
    UpdateMatrix();
}

void GameCamera::Update() {
    UpdateMatrix();
}

void GameCamera::UpdateMatrix() {
    if (isOrthographic_) {
        UpdateMatrixOrthographic();
    } else {
        Camera::UpdateMatrix();
    }
}

void GameCamera::InitializeOrthographic(int kClientWidth, int kClientHeight, float viewWidth, float viewHeight) {
    kClientWidth_ = kClientWidth;
    kClientHeight_ = kClientHeight;
    isOrthographic_ = true;
    orthoWidth_ = viewWidth;
    orthoHeight_ = viewHeight;

    // 2Dモード：カメラはZ軸上から見下ろす（Z=-10の位置からZ=0を見る）
    transform_.translate = { 0.0f, 5.0f, -10.0f };
    transform_.rotate = { 0.0f, 0.0f, 0.0f };

    // 設定ファイルがあれば読み込む
    LoadConfig();

    UpdateMatrixOrthographic();
}

void GameCamera::UpdateMatrixOrthographic() {
    // ターゲットへの追従（ルームベース遷移）
    if (followTarget_) {
        // ターゲットの現在の座標
        float targetX = followTarget_->x;
        float targetY = followTarget_->y;

        // カスタム境界線リストを用いたルーム計算
        int newRoomX = currentRoomX_;
        int newRoomY = currentRoomY_;
        float cameraTargetX = transform_.translate.x;
        float cameraTargetY = transform_.translate.y;

        if (!boundaryX_.empty() && !boundaryY_.empty()) {
            // X軸の区間検索
            auto itX = std::lower_bound(boundaryX_.begin(), boundaryX_.end(), targetX);
            if (itX != boundaryX_.end() && itX != boundaryX_.begin()) {
                newRoomX = static_cast<int>(std::distance(boundaryX_.begin(), itX)) - 1;
                float minX = *(itX - 1);
                float maxX = *itX;
                // 区間がカメラサイズより大きい場合は、ターゲットを中心にクランプする
                if (maxX - minX > orthoWidth_) {
                    float halfW = orthoWidth_ * 0.5f;
                    cameraTargetX = std::clamp(targetX, minX + halfW, maxX - halfW);
                } else {
                    // カメラサイズより小さい場合は区間の中心
                    cameraTargetX = (minX + maxX) * 0.5f;
                }
            } else if (itX == boundaryX_.begin()) {
                newRoomX = -1; // 範囲外左
                cameraTargetX = boundaryX_.front() - orthoWidth_ * 0.5f;
            } else {
                newRoomX = static_cast<int>(boundaryX_.size()); // 範囲外右
                cameraTargetX = boundaryX_.back() + orthoWidth_ * 0.5f;
            }

            // Y軸の区間検索
            auto itY = std::lower_bound(boundaryY_.begin(), boundaryY_.end(), targetY);
            if (itY != boundaryY_.end() && itY != boundaryY_.begin()) {
                newRoomY = static_cast<int>(std::distance(boundaryY_.begin(), itY)) - 1;
                float minY = *(itY - 1);
                float maxY = *itY;
                if (maxY - minY > orthoHeight_) {
                    float halfH = orthoHeight_ * 0.5f;
                    cameraTargetY = std::clamp(targetY, minY + halfH, maxY - halfH);
                } else {
                    cameraTargetY = (minY + maxY) * 0.5f;
                }
            } else if (itY == boundaryY_.begin()) {
                newRoomY = -1; // 範囲外下
                cameraTargetY = boundaryY_.front() - orthoHeight_ * 0.5f;
            } else {
                newRoomY = static_cast<int>(boundaryY_.size()); // 範囲外上
                cameraTargetY = boundaryY_.back() + orthoHeight_ * 0.5f;
            }
        }

        if (newRoomX != currentRoomX_ || newRoomY != currentRoomY_) {
            currentRoomX_ = newRoomX;
            currentRoomY_ = newRoomY;
            isTransitioning_ = true;
        }

        // カメラが目標位置へスライド移動する
        // Lerpでの追従
        transform_.translate.x += (cameraTargetX - transform_.translate.x) * transitionLerp_;
        transform_.translate.y += (cameraTargetY - transform_.translate.y) * transitionLerp_;

        // 目標位置に十分近づいたら遷移終了
        float distX = std::abs(transform_.translate.x - cameraTargetX);
        float distY = std::abs(transform_.translate.y - cameraTargetY);
        if (isTransitioning_ && distX < 0.05f && distY < 0.05f) {
            transform_.translate.x = cameraTargetX;
            transform_.translate.y = cameraTargetY;
            isTransitioning_ = false;
        }
    }

    // ビュー行列：カメラ位置と3D回転（X, Y, Z）を反映
    Matrix4x4 rotationMatrix = TransformFunctions::Multiply(
        TransformFunctions::Multiply(
            TransformFunctions::MakeRoteZMatrix(transform_.rotate.z),
            TransformFunctions::MakeRoteXMatrix(transform_.rotate.x)
        ),
        TransformFunctions::MakeRoteYMatrix(transform_.rotate.y)
    );

    Matrix4x4 translateMatrix = TransformFunctions::MakeTranslateMatrix(
        { -transform_.translate.x, -transform_.translate.y, -transform_.translate.z }
    );

    Matrix4x4 rotateMatrixInv = TransformFunctions::Transpose(rotationMatrix);

    viewMatrix_ = TransformFunctions::Multiply(translateMatrix, rotateMatrixInv);

    // 正射影行列
    float halfW = orthoWidth_ * 0.5f;
    float halfH = orthoHeight_ * 0.5f;
    projectionMatrix_ = TransformFunctions::MakeOrthographicMatrix(
        -halfW, halfH, halfW, -halfH, 0.1f, 100.0f
    );

    // CameraManagerにも反映
    CameraManager::GetInstance()->SetCameraInfo(transform_.translate, viewMatrix_, projectionMatrix_);
}

void GameCamera::LoadConfig(const std::string& filepath) {
    if (!std::filesystem::exists(filepath)) {
        return;
    }
    std::ifstream ifs(filepath);
    if (!ifs.is_open()) {
        return;
    }
    try {
        nlohmann::json j;
        ifs >> j;
        if (j.contains("scale")) {
            SetScale(j["scale"].get<float>());
        } else {
            if (j.contains("orthoWidth")) orthoWidth_ = j["orthoWidth"].get<float>();
            if (j.contains("orthoHeight")) orthoHeight_ = j["orthoHeight"].get<float>();
        }
        if (j.contains("followLerp")) followLerp_ = j["followLerp"].get<float>();
        if (j.contains("transitionLerp")) transitionLerp_ = j["transitionLerp"].get<float>();
        
        if (j.contains("rotate")) {
            transform_.rotate.x = j["rotate"]["x"].get<float>();
            transform_.rotate.y = j["rotate"]["y"].get<float>();
            transform_.rotate.z = j["rotate"]["z"].get<float>();
        }
    } catch (...) {
        // エラー時はデフォルト値のままにする
    }
    ifs.close();
}

void GameCamera::SaveConfig(const std::string& filepath) {
    std::filesystem::path path(filepath);
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }

    std::ofstream ofs(filepath);
    if (!ofs.is_open()) {
        return;
    }
    nlohmann::json j;
    j["scale"] = scale_;
    j["orthoWidth"] = orthoWidth_;
    j["orthoHeight"] = orthoHeight_;
    j["followLerp"] = followLerp_;
    j["transitionLerp"] = transitionLerp_;

    j["rotate"]["x"] = transform_.rotate.x;
    j["rotate"]["y"] = transform_.rotate.y;
    j["rotate"]["z"] = transform_.rotate.z;

    ofs << j.dump(4);
    ofs.close();
}
