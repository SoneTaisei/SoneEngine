#include "GameCameraController.h"
#include "Graphics/Camera/GameCamera.h"
#include "Game2D/Player/Player2D.h"
#include "Game2D/MapChip2D.h"
#include "Input/GamepadInput.h"
#include "Core/Utility/ParameterManager.h"
#include <cmath>

void GameCameraController::Setup(GameCamera* camera, Player2D* player, MapChip2D* map) {
    camera_ = camera;
    if (!camera_) return;

    float orthoWidth = ParameterManager::GetInstance()->GetValue("GameScene", "orthoWidth", 20.0f);
    float orthoHeight = ParameterManager::GetInstance()->GetValue("GameScene", "orthoHeight", 11.25f);
    camera_->InitializeOrthographic(1280, 720, orthoWidth, orthoHeight);

    if (map) {
        camera_->SetRooms(map->GetRooms());
    }
    if (player) {
        camera_->SetFollowTarget(&player->GetPosition());
    }
    camera_->SetFollowEnabled(true);
    camera_->SnapToTarget();
    initialCameraScale_ = camera_->GetScale();
    cameraLookRotation_ = { 0.0f, 0.0f, 0.0f };
}

void GameCameraController::Update(float deltaTime, Player2D* player, MapChip2D* map, bool isClear, bool isReplaying) {
    (void)deltaTime;
    if (!camera_) return;

    if (player && !isReplaying) {
        if (player->IsDead()) {
            camera_->SetFollowTarget(nullptr);
        } else {
            camera_->SetFollowTarget(&player->GetPosition());
        }
    }

    if (map) {
        if (isClear) {
            camera_->SetRooms({}); // クリア時は部屋制限を解除してプレイヤー中心へ直接追従
        } else {
            camera_->SetRooms(map->GetRooms());
        }
    }

    // 右スティック（RStick）によるカメラ見渡し回転
    auto gamepad = GamepadInput::GetInstance();
    if (gamepad && gamepad->IsConnected()) {
        Vector2 rStick = gamepad->GetRightStick();
        if (std::abs(rStick.x) > 0.15f || std::abs(rStick.y) > 0.15f) {
            float targetYaw = -rStick.x * 0.6f;   // 左右見渡し（水平回転）
            float targetPitch = rStick.y * 0.4f;  // 上下見渡し（垂直チルト）
            cameraLookRotation_.y += (targetYaw - cameraLookRotation_.y) * 0.1f;
            cameraLookRotation_.x += (targetPitch - cameraLookRotation_.x) * 0.1f;
        } else {
            cameraLookRotation_.y += (0.0f - cameraLookRotation_.y) * 0.1f;
            cameraLookRotation_.x += (0.0f - cameraLookRotation_.x) * 0.1f;
        }
        Vector3 baseRot = camera_->GetBaseRotation();
        camera_->SetRotation({
            baseRot.x + cameraLookRotation_.x,
            baseRot.y + cameraLookRotation_.y,
            baseRot.z
        });
    }
}

void GameCameraController::OnRespawn(MapChip2D* map) {
    if (!camera_) return;
    if (map) {
        camera_->SetRooms(map->GetRooms());
    }
    camera_->SnapToTarget();
}

void GameCameraController::ResetView() {
    if (!camera_) return;
    camera_->SetScale(initialCameraScale_);
    camera_->SetFollowOffset({ 0.0f, 0.0f, 0.0f });
}

void GameCameraController::UpdateEditor(Player2D* player, MapChip2D* map) {
    if (!camera_ || !player) return;

    if (!camera_->IsOrthographic()) {
        float orthoWidth = ParameterManager::GetInstance()->GetValue("GameScene", "orthoWidth", 20.0f);
        float orthoHeight = ParameterManager::GetInstance()->GetValue("GameScene", "orthoHeight", 11.25f);
        camera_->InitializeOrthographic(1280, 720, orthoWidth, orthoHeight);
    }
    if (map) {
        camera_->SetRooms(map->GetRooms());
    }
    camera_->SetFollowTarget(&player->GetPosition());
    camera_->SetFollowEnabled(true);
    camera_->SnapToTarget();
}
