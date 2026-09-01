#include "GameCamera.h"
#include "CameraManager.h"
#include "Core/Utility/TransformFunctions.h"
#include "Core/TimeManager.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include <cstdlib>
#include <ctime>

void GameCamera::Initialize(int kClientWidth, int kClientHeight) {
    Camera::Initialize(kClientWidth, kClientHeight);
    // 初期位置の設定（例えばプレイヤーの少し後ろ）
    transform_.translate = { 0.0f, 0.0f, -10.0f };
    transform_.rotate = { 0.0f, 0.0f, 0.0f };
    UpdateMatrix();
}

void GameCamera::Update() {
    // 画面揺れの更新
    float deltaTime = TimeManager::GetInstance().GetDeltaTime();
    if (shakeTimer_ > 0.0f) {
        shakeTimer_ -= deltaTime;
        if (shakeTimer_ <= 0.0f) {
            shakeTimer_ = 0.0f;
            shakeOffset_ = {0.0f, 0.0f, 0.0f};
        } else {
            // 残り時間に応じて揺れを減衰させる
            float currentStrength = shakeStrength_ * (shakeTimer_ / shakeDuration_);
            // ランダムに揺らす
            float rx = ((float)std::rand() / RAND_MAX * 2.0f - 1.0f) * currentStrength;
            float ry = ((float)std::rand() / RAND_MAX * 2.0f - 1.0f) * currentStrength;
            shakeOffset_ = {rx, ry, 0.0f};
        }
    }
    UpdateMatrix();
}

void GameCamera::Shake(float strength, float duration) {
    shakeStrength_ = strength;
    shakeDuration_ = duration;
    shakeTimer_ = duration;
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

        if (!rooms_.empty()) {
            int currentRoomIndex = -1;
            // まずは現在ターゲットがいるルームを探す
            for (int i = 0; i < rooms_.size(); ++i) {
                const auto& r = rooms_[i];
                if (targetX >= r.x && targetX <= r.x + r.width &&
                    targetY >= r.y && targetY <= r.y + r.height) {
                    
                    // 以前と同じルームならそれを優先
                    if (currentRoomX_ == i) {
                        currentRoomIndex = i;
                        break;
                    }
                    if (currentRoomIndex == -1) {
                        currentRoomIndex = i;
                    }
                }
            }

            // どのルームにも属していない場合は、一番近いルームを探す
            if (currentRoomIndex == -1) {
                float minDistSq = 1e10f;
                for (int i = 0; i < rooms_.size(); ++i) {
                    const auto& r = rooms_[i];
                    float centerX = r.x + r.width * 0.5f;
                    float centerY = r.y + r.height * 0.5f;
                    float dx = targetX - centerX;
                    float dy = targetY - centerY;
                    float distSq = dx * dx + dy * dy;
                    if (distSq < minDistSq) {
                        minDistSq = distSq;
                        currentRoomIndex = i;
                    }
                }
            }

            if (currentRoomIndex != -1) {
                newRoomX = currentRoomIndex; // currentRoomX_ にルームのインデックスを入れる
                newRoomY = 0; // 使わないが0にしておく

                const auto& activeRoom = rooms_[currentRoomIndex];
                float halfW = orthoWidth_ * 0.5f;
                float halfH = orthoHeight_ * 0.5f;
                float minClampX = activeRoom.x + halfW;
                float maxClampX = activeRoom.x + activeRoom.width - halfW;
                float minClampY = activeRoom.y + halfH;
                float maxClampY = activeRoom.y + activeRoom.height - halfH;

                if (minClampX > maxClampX) {
                    cameraTargetX = activeRoom.x + activeRoom.width * 0.5f;
                } else {
                    cameraTargetX = std::clamp(targetX, minClampX, maxClampX);
                }

                if (minClampY > maxClampY) {
                    cameraTargetY = activeRoom.y + activeRoom.height * 0.5f;
                } else {
                    cameraTargetY = std::clamp(targetY, minClampY, maxClampY);
                }
            } else {
                cameraTargetX = targetX;
                cameraTargetY = targetY;
            }
        } else {
            // ルーム設定がない場合はターゲット（プレイヤー）を直接追従
            cameraTargetX = targetX;
            cameraTargetY = targetY;
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
        { -(transform_.translate.x + shakeOffset_.x), -(transform_.translate.y + shakeOffset_.y), -transform_.translate.z }
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
