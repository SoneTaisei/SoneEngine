#include "Player2D.h"
#include "Renderer/DirectXCommon/DirectXCommon.h"
#include "../MapChip2D.h"
#include "Core/Utility/TransformFunctions.h"
#include "Graphics/TextureManager.h"
#include "Core/TimeManager.h"
#include "Resource/Model/ModelManager.h"
#include <cmath>
#include <algorithm>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

void Player2D::Initialize() {
    PlayerConfig::Load(params_, "resources/json/shared/Player/player_parameters.json");

    Microsoft::WRL::ComPtr<ID3D12Device> device = DirectXCommon::GetInstance()->GetDevice();
    Primitive* boxPrimitive = PrimitiveManager::GetInstance()->GetPrimitive(PrimitiveType::Box, 1.0f);
    uint32_t texHandle = TextureManager::GetInstance()->Load("resources/Object/School/human/white.png");
    Primitive* ringPrimitive = PrimitiveManager::GetInstance()->GetRing(0.8f, 1.0f, 32, 0.0f, 2.0f * 3.14159f, {1,1,1,1}, {1,1,1,1}, false);
    
    Model* playerModel = ModelManager::GetInstance()->GetModel("resources/Object/Original/player", "Player.gltf");

    visuals_.Initialize(device.Get(), boxPrimitive, ringPrimitive, texHandle, playerModel);
}

void Player2D::FindSpawnPoint(const MapChip2D& map) {
    if (map.HasPlayerSpawn()) {
        state_.startPosition_ = map.GetPlayerSpawnWorldPosition(state_.startPosition_);
        state_.position_ = state_.startPosition_;
        if (gameObject_) {
            if (auto* tc = gameObject_->GetComponent<TransformComponent>()) {
                tc->SetPosition(state_.position_);
            }
        }
    }
}

void Player2D::UpdateWithMap(MapChip2D& map, bool isTransitioning) {
    (void)map;
    (void)isTransitioning;

    float deltaTime = TimeManager::GetInstance().GetDeltaTime();

    // ゴール時の待機
    if (state_.isGoal_) {
        state_.goalTimer_ += deltaTime;
        state_.velocity_ = { 0.0f, 0.0f, 0.0f };
        visuals_.Update(state_, params_, deltaTime);
        return;
    }

    // 死亡時・リスポーン処理
    if (state_.isDead_) {
        state_.deathTimer_ += deltaTime;
        if (!state_.isRespawning_) {
            if (state_.deathTimer_ >= params_.deathDuration_) {
                state_.isRespawning_ = true;
                state_.respawnTimer_ = 0.0f;
                state_.position_ = state_.startPosition_;
                state_.velocity_ = { 0.0f, 0.0f, 0.0f };
                state_.launchVelocityX_ = 0.0f; // 空中で死んだ時の発射の勢いをリスポーン先に持ち込まない
            }
        } else {
            state_.respawnTimer_ += deltaTime;
            if (state_.respawnTimer_ >= params_.respawnDuration_) {
                state_.isDead_ = false;
                state_.isRespawning_ = false;
                state_.isOnGround_ = false;
            }
        }
        visuals_.Update(state_, params_, deltaTime);
        return;
    }

    // 入力の更新
    input_.Update(currentInput_);
    // 鎖アクション（スピン中など）による入力修飾
    currentInput_.moveX *= actionMoveFactor_;
    if (actionJumpLocked_) {
        currentInput_.isJumpPressed = false;
        currentInput_.isJumpHeld = false;
    }

    // 物理・移動・当たり判定の更新
    physics_.Update(state_, params_, currentInput_, deltaTime, this, &map);

    // 見た目・Transformの同期
    visuals_.Update(state_, params_, deltaTime);

    if (gameObject_) {
        if (auto* tc = gameObject_->GetComponent<TransformComponent>()) {
            tc->SetPosition(state_.position_);
        }
    }
}

void Player2D::Update() {
    float deltaTime = TimeManager::GetInstance().GetDeltaTime();
    visuals_.Update(state_, params_, deltaTime);
}

void Player2D::Draw() {
    visuals_.Draw(state_, params_);
}

void Player2D::ResetState(const Vector3& initPos) {
    state_.position_ = initPos;
    state_.velocity_ = { 0.0f, 0.0f, 0.0f };
    state_.launchVelocityX_ = 0.0f;
    state_.isOnGround_ = false;
    state_.isDead_ = false;
    state_.deathTimer_ = 0.0f;
    state_.isRespawning_ = false;
    state_.respawnTimer_ = 0.0f;
    state_.isGoal_ = false;
    state_.goalTimer_ = 0.0f;
    state_.chainLength_ = 3;
}

void Player2D::DisplayImGui() {
#ifdef USE_IMGUI
    if (ImGui::TreeNode("Player Status")) {
        ImGui::Text("Chain Length : %d", state_.chainLength_);
        ImGui::TreePop();
    }
    if (ImGui::TreeNode("Player Parameters")) {
        ImGui::DragFloat("Move Speed", &params_.moveSpeed_, 0.1f, 0.0f, 30.0f);
        ImGui::DragFloat("Jump Power", &params_.jumpPower_, 0.1f, 0.0f, 50.0f);
        ImGui::DragFloat("Gravity", &params_.gravity_, 0.5f, -100.0f, 0.0f);
        ImGui::DragFloat("Max Fall Speed", &params_.maxFallSpeed_, 0.5f, -100.0f, 0.0f);
        ImGui::DragFloat("Half Width", &params_.halfWidth_, 0.01f, 0.05f, 2.0f);
        ImGui::DragFloat("Half Height", &params_.halfHeight_, 0.01f, 0.05f, 2.0f);
        ImGui::ColorEdit4("Player Color", &params_.colorNormal_.x);
        ImGui::DragFloat("Chain Jump Penalty", &params_.chainJumpPenalty_, 0.1f, 0.0f, 10.0f);
        ImGui::DragFloat("Model Scale", &params_.modelScale_, 0.05f, 0.5f, 10.0f);

        if (ImGui::Button("Save Parameters")) {
            PlayerConfig::Save(params_, "resources/json/shared/Player/player_parameters.json");
        }
        ImGui::SameLine();
        if (ImGui::Button("Load Parameters")) {
            PlayerConfig::Load(params_, "resources/json/shared/Player/player_parameters.json");
        }
        ImGui::TreePop();
    }

    visuals_.DisplayImGui();
#endif
}
