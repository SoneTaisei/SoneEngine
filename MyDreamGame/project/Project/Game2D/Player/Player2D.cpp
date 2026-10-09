#include "Player2D.h"
#include "States/PlayerStates.h"
#include "Renderer/DirectXCommon/DirectXCommon.h"
#include "../MapChip2D.h"
#include "Graphics/TextureManager.h"
#include "Editor/Replay/ReplayManager.h"
#include "Resource/Model/ModelManager.h"
#include <cmath>
#include <algorithm>
#include <filesystem>
#ifdef USE_IMGUI
#include "../../externals/imgui/imgui.h"
#endif

void Player2D::Initialize() {
    Log("Player2D::Initialize: Start\n");
    PlayerConfig::Load(params_, "resources/json/shared/Player/player_parameters.json");
    Log("Player2D::Initialize: Config loaded\n");

    Microsoft::WRL::ComPtr<ID3D12Device> device;
    device = DirectXCommon::GetInstance()->GetDevice();

    Log("Player2D::Initialize: Getting Primitive\n");
    Primitive* boxPrimitive = PrimitiveManager::GetInstance()->GetPrimitive(PrimitiveType::Box, 1.0f);
    Log("Player2D::Initialize: Loading Texture\n");
    uint32_t texHandle = TextureManager::GetInstance()->Load("resources/Object/School/human/white.png");
    Log("Player2D::Initialize: Getting Ring Primitive\n");
    Primitive* ringPrimitive = PrimitiveManager::GetInstance()->GetRing(0.8f, 1.0f, 32, 0.0f, 2.0f * 3.14159f, {1,1,1,1}, {1,1,1,1}, false);
    
    Log("Player2D::Initialize: Loading Player 3D Model\n");
    Model* playerModel = ModelManager::GetInstance()->GetModel("resources/Object/Original/gaikotu", "scene.gltf");
    if (playerModel) {
        uint32_t playerTexIndex = TextureManager::GetInstance()->Load("resources/Object/Original/gaikotu/textures/mini_simple_material_primary_baseColor.png");
        playerModel->SetTextureHandle(TextureManager::GetInstance()->GetGpuHandle(playerTexIndex));
    }

    Log("Player2D::Initialize: Init Visuals\n");
    visuals_.Initialize(device.Get(), boxPrimitive, ringPrimitive, texHandle, playerModel);

    // 初期ステートの設定（ポリモーフィズム）
    ChangeState(std::make_unique<PlayerNormalState>());

    Log("Player2D::Initialize: Finish\n");
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

void Player2D::UpdateWithMap(MapChip2D& map, bool isTransitioning, bool canControl) {
    input_.Update(currentInput_);
    if (!canControl) {
        currentInput_ = InputState{};
        state_.velocity_.x = 0.0f;
    }
    // パラメータに基づいてPrimitiveObjectのスケールを常に反映させる（JSONロード時のバグ対策）
    // リプレイ再生中でかつ一時停止中の場合、物理演算や各種タイマー進行を停止する
    if (ReplayManager::GetInstance()->IsPlaying() && ReplayManager::GetInstance()->IsPaused()) {
        state_.stuckTimer_ = 0.0f;
        state_.prevPositionForBugCheck_ = state_.position_;
        visuals_.SyncTransform(state_.position_);
        if (gameObject_) {
            if (auto* tc = gameObject_->GetComponent<TransformComponent>()) {
                tc->SetPosition(state_.position_);
            }
        }
        return;
    }

    float deltaTime = TimeManager::GetInstance().GetDeltaTime();
    
    // パーティクルや見た目のベース更新 (早めに呼んでおく)
    visuals_.Update(state_, params_, deltaTime);

    // カメラスライド（ルーム遷移）中の硬直処理
    if (isTransitioning) {
        visuals_.SyncTransform(state_.position_);
        return;
    }

    // ステートマシンによるポリモーフィックな更新処理
    if (currentState_) {
        currentState_->Update(this, map, deltaTime);
    }

    state_.prevPositionForBugCheck_ = state_.position_;

    // 描画座標を更新
    visuals_.SyncTransform(state_.position_);

    if (gameObject_) {
        if (auto* tc = gameObject_->GetComponent<TransformComponent>()) {
            tc->SetPosition(state_.position_);
        }
    }
}

void Player2D::Draw() {
    visuals_.Draw(state_, params_);
}

void Player2D::DisplayImGui() {
#ifdef USE_IMGUI
    bool isTreeNodeOpen = ImGui::TreeNode("プレイヤー2D (Player2D)");
    static bool wasTreeNodeOpen = false;

    // ツリーノードが開かれた瞬間にJSONをロードする
    if (isTreeNodeOpen && !wasTreeNodeOpen) {
        PlayerConfig::Load(params_, "resources/json/shared/Player/player_parameters.json");
    }
    wasTreeNodeOpen = isTreeNodeOpen;

    if (isTreeNodeOpen) {
        if (ImGui::Button("パラメータ保存 (Save)")) {
            std::filesystem::create_directories("resources/json/shared/Player");
            PlayerConfig::Save(params_, "resources/json/shared/Player/player_parameters.json");
        }
        ImGui::SameLine();
        if (ImGui::Button("パラメータ読込 (Load)")) {
            PlayerConfig::Load(params_, "resources/json/shared/Player/player_parameters.json");
        }

        ImGui::DragFloat3("座標", &state_.position_.x, 0.1f);
        ImGui::DragFloat3("速度", &state_.velocity_.x, 0.1f);

        if (currentState_) {
            if (dynamic_cast<PlayerNormalState*>(currentState_.get())) {
                ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "現在の状態: 通常 (PlayerNormalState)");
            } else if (dynamic_cast<PlayerDeadState*>(currentState_.get())) {
                ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "現在の状態: 死亡演出 (PlayerDeadState)");
            } else if (dynamic_cast<PlayerRespawnState*>(currentState_.get())) {
                ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "現在の状態: リスポーン演出 (PlayerRespawnState)");
            } else if (dynamic_cast<PlayerGoalState*>(currentState_.get())) {
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "現在の状態: ゴール演出 (PlayerGoalState)");
            }
        }
        
        ImGui::Text("接地状態: %s", state_.isOnGround_ ? "True" : "False");
        ImGui::Text("壁スライド: %s", state_.isWallSliding_ ? "True" : "False");
        ImGui::Text("壁しがみつき: %s", state_.isWallClinging_ ? "True" : "False");
        ImGui::Text("右壁接触: %s", state_.isTouchingWallRight_ ? "True" : "False");
        ImGui::Text("左壁接触: %s", state_.isTouchingWallLeft_ ? "True" : "False");
        
        ImGui::Text("現在のスタミナ: %.1f / %.1f", state_.stamina_, params_.maxStamina_);
        if (state_.isExhausted_) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "(疲労状態)");
        }

        if (ImGui::TreeNode("基本移動")) {
            ImGui::DragFloat("移動速度", &params_.moveSpeed_, 0.1f, 0.0f, 30.0f);
            ImGui::DragFloat("ジャンプ力", &params_.jumpPower_, 0.1f, 0.0f, 30.0f);
            ImGui::DragFloat("重力", &params_.gravity_, 0.1f, -100.0f, 0.0f);
            ImGui::DragFloat("最大落下速度", &params_.maxFallSpeed_, 0.1f, -100.0f, 0.0f);
            ImGui::TreePop();
        }
        
        if (ImGui::TreeNode("ダッシュ")) {
            ImGui::DragFloat("ダッシュ継続時間", &params_.dashDuration_, 0.01f, 0.0f, 2.0f);
            ImGui::DragFloat("ダッシュ速度", &params_.dashSpeed_, 0.1f, 0.0f, 50.0f);
            ImGui::DragFloat("ダッシュ終了時の上向き速度", &params_.dashEndUpwardVelocity_, 0.1f, 0.0f, 30.0f);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("壁アクション")) {
            ImGui::DragFloat("壁ジャンプ後の速度補間時間", &params_.wallJumpDuration_, 0.01f, 0.0f, 2.0f);
            ImGui::DragFloat2("壁ジャンプの力 (X,Y)", &params_.wallJumpPower_.x, 0.1f, 0.0f, 50.0f);
            ImGui::DragFloat("壁ジャンプ後の壁方向入力制限時間", &params_.wallJumpDirLockDuration_, 0.01f, 0.0f, 2.0f);
            ImGui::DragFloat("壁ずり落ち時の落下速度", &params_.wallSlideSpeed_, 0.1f, -50.0f, 0.0f);
            ImGui::DragFloat("崖登り着地停止時間", &params_.climbLandingDuration_, 0.01f, 0.0f, 1.0f);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("スタミナ (Stamina)")) {
            ImGui::DragFloat("最大スタミナ", &params_.maxStamina_, 1.0f, 0.0f, 500.0f);
            ImGui::DragFloat("壁張り付き時消費量/秒", &params_.staminaConsumeCling_, 0.5f, 0.0f, 100.0f);
            ImGui::DragFloat("壁登り時消費量/秒", &params_.staminaConsumeClimb_, 0.5f, 0.0f, 100.0f);
            ImGui::DragFloat("壁ジャンプ時消費量", &params_.staminaConsumeJump_, 0.5f, 0.0f, 100.0f);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("演出・ゲームルール")) {
            ImGui::DragFloat("死亡演出時間", &params_.deathDuration_, 0.01f, 0.0f, 5.0f);
            ImGui::DragFloat("リスポーン時間", &params_.respawnDuration_, 0.01f, 0.0f, 5.0f);
            ImGui::DragFloat("ゴール待機時間", &params_.goalWaitTime_, 0.01f, 0.0f, 10.0f);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("色")) {
            ImGui::ColorEdit4("通常カラー", &params_.colorNormal_.x);
            ImGui::ColorEdit4("ダッシュカラー", &params_.colorDashed_.x);
            ImGui::ColorEdit4("疲労カラー", &params_.colorTired_.x);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("サイズ")) {
            bool sizeChanged = false;
            if (ImGui::DragFloat("当たり判定の半幅", &params_.halfWidth_, 0.01f, 0.05f, 5.0f)) {
                sizeChanged = true;
            }
            if (ImGui::DragFloat("当たり判定の半高", &params_.halfHeight_, 0.01f, 0.05f, 5.0f)) {
                sizeChanged = true;
            }
            if (sizeChanged) {
                visuals_.SyncSize(params_);
            }
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("モデルしがみつきポーズ調整 (Cling Debug)")) {
            visuals_.DisplayImGui();
            ImGui::TreePop();
        }

        ImGui::TreePop();
    }
#endif
}




































void Player2D::ResetState(const Vector3& initPos) {
    state_.position_ = initPos;
    state_.velocity_ = { 0.0f, 0.0f, 0.0f };
    state_.isOnGround_ = false;
    state_.canDash_ = true;
    state_.isDashing_ = false;
    state_.dashTimer_ = 0.0f;
    state_.isTouchingWallRight_ = false;
    state_.isTouchingWallLeft_ = false;
    state_.wallJumpDirLockTimer_ = 0.0f;
    state_.lockedDirectionX_ = 0.0f;
    state_.wallJumpTimer_ = 0.0f;
    state_.isWallSliding_ = false;
    state_.isWallClinging_ = false;
    state_.isDead_ = false;
    state_.deathTimer_ = 0.0f;
    state_.isGoal_ = false;
    state_.goalTimer_ = 0.0f;
    state_.score_ = 0;
    state_.stuckTimer_ = 0.0f;
    state_.inWallTimer_ = 0.0f;
    state_.springControlDisableTimer_ = 0.0f;
    state_.hitstopTimer_ = 0.0f;
    state_.climbingUpTimer_ = 0.0f;
    state_.climbLandingTimer_ = 0.0f;
    
    visuals_.ResetVisuals(state_.position_, params_);
    visuals_.ClearEffects();

    // 状態を通常ステートへリセット（ポリモーフィズム）
    ChangeState(std::make_unique<PlayerNormalState>());
}

void Player2D::Kill(bool isFallDeath) {
    if (!state_.isDead_) {
        ChangeState(std::make_unique<PlayerDeadState>(isFallDeath));
    }
}

void Player2D::ReachGoal() {
    if (!state_.isGoal_) {
        ChangeState(std::make_unique<PlayerGoalState>());
    }
}

void Player2D::ChangeState(std::unique_ptr<IPlayerState> nextState) {
    if (currentState_) {
        currentState_->Exit(this);
    }
    currentState_ = std::move(nextState);
    if (currentState_) {
        currentState_->Enter(this);
    }
}

AABB2D Player2D::GetAABB() const {
    return physics_.GetAABB(state_, params_);
}

void Player2D::Update() {
    // IComponentとしてのUpdateは現在使用せず、UpdateWithMapを使用する
}
