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
            float minX = -100000.0f;
            float maxX = 100000.0f;
            auto itX = std::lower_bound(boundaryX_.begin(), boundaryX_.end(), targetX);
            if (itX == boundaryX_.begin()) {
                newRoomX = -1;
                maxX = *itX;
            } else if (itX == boundaryX_.end()) {
                newRoomX = static_cast<int>(boundaryX_.size());
                minX = *(itX - 1);
            } else {
                newRoomX = static_cast<int>(std::distance(boundaryX_.begin(), itX)) - 1;
                minX = *(itX - 1);
                maxX = *itX;
            }

            float halfW = orthoWidth_ * 0.5f;
            float minClampX = minX + halfW;
            float maxClampX = maxX - halfW;
            if (minClampX > maxClampX) {
                cameraTargetX = (minX + maxX) * 0.5f;
            } else {
                cameraTargetX = std::clamp(targetX, minClampX, maxClampX);
            }

            // Y軸の区間検索
            float minY = -100000.0f;
            float maxY = 100000.0f;
            auto itY = std::lower_bound(boundaryY_.begin(), boundaryY_.end(), targetY);
            if (itY == boundaryY_.begin()) {
                newRoomY = -1;
                maxY = *itY;
            } else if (itY == boundaryY_.end()) {
                newRoomY = static_cast<int>(boundaryY_.size());
                minY = *(itY - 1);
            } else {
                newRoomY = static_cast<int>(std::distance(boundaryY_.begin(), itY)) - 1;
                minY = *(itY - 1);
                maxY = *itY;
            }

            float halfH = orthoHeight_ * 0.5f;
            float minClampY = minY + halfH;
            float maxClampY = maxY - halfH;
            if (minClampY > maxClampY) {
                // Y軸は下がプラス（値が大きい）ため、下限の境界は maxY となる。
                // 画面より区間が狭い場合、下限に画面の下端をピッタリ合わせるために maxClampY を優先する。
                cameraTargetY = maxClampY;
            } else {
                cameraTargetY = std::clamp(targetY, minClampY, maxClampY);
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
