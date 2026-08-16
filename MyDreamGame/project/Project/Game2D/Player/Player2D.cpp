#include "Player2D.h"
#include "Renderer/DirectXCommon/DirectXCommon.h"
#include "../MapChip2D.h"
#include "Core/Utility/TransformFunctions.h"
#include "Graphics/TextureManager.h"
#include "Core/TimeManager.h"
#include "Input/KeyboardInput.h"
#include "Editor/Replay/ReplayManager.h"
#include "Resource/Model/ModelManager.h"
#include <cmath>
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <iostream>
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
    Log("Player2D::Initialize: Finish\n");
}

void Player2D::FindSpawnPoint(const MapChip2D& map) {
    for (int y = 0; y < map.GetHeight(); ++y) {
        for (int x = 0; x < map.GetWidth(); ++x) {
            if (map.GetChipType(x, y) == MapChip2D::ChipType::kPlayerSpawn) {
                // スポーン地点の中心座標を計算
                state_.startPosition_.x = map.ChipToWorldX(x) + map.GetChipSize() * 0.5f;
                state_.startPosition_.y = map.ChipToWorldY(y) + map.GetChipSize() * 0.5f;
                state_.position_ = state_.startPosition_;
                if (gameObject_) {
                    if (auto* tc = gameObject_->GetComponent<TransformComponent>()) {
                        tc->SetPosition(state_.position_);
                    }
                }
                return;
            }
        }
    }
}

void Player2D::UpdateWithMap(MapChip2D& map, bool isTransitioning) {
    input_.Update(currentInput_);
    // パラメータに基づいてPrimitiveObjectのスケールを常に反映させる（JSONロード時のバグ対策）
    if (!state_.isRespawning_ && visuals_.GetPrimitiveObject()) {
        visuals_.GetPrimitiveObject()->SetScale({ params_.halfWidth_ * 2.0f, params_.halfHeight_ * 2.0f, 1.0f });
    }

    // リプレイ再生中でかつ一時停止中の場合、物理演算や各種タイマー進行を停止する
    if (ReplayManager::GetInstance()->IsPlaying() && ReplayManager::GetInstance()->IsPaused()) {
        state_.stuckTimer_ = 0.0f;
        state_.prevPositionForBugCheck_ = state_.position_;
        if (visuals_.GetPrimitiveObject()) {
            visuals_.GetPrimitiveObject()->SetTranslation(state_.position_);
            visuals_.GetPrimitiveObject()->Update();
        }
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

    if (state_.isGoal_) {
        // ゴール演出時のプレイヤーは静止させる
        state_.goalTimer_ += deltaTime;
        state_.velocity_.x = 0.0f;
        state_.velocity_.y = 0.0f;
        
        // 紙吹雪パーティクルの更新は visuals_.Update 内で行う
        return;
    }

    // 死亡演出中の更新処理
    if (state_.isDead_) {
        // スローモーション中は実時間が異なるため、deathTimer_にはdeltaTimeを足していく
        state_.deathTimer_ += deltaTime;

        // ノックバック物理挙動（演出中ずっと続ける）
        state_.velocity_.y += params_.gravity_ * deltaTime;
        state_.position_.x += state_.velocity_.x * deltaTime;
        state_.position_.y += state_.velocity_.y * deltaTime;

        // ディゾルブ演出の進行
        float t = (std::min)(state_.deathTimer_ / params_.deathDuration_, 1.0f);
        if (visuals_.GetPrimitiveObject()) {
            visuals_.GetPrimitiveObject()->GetMaterial().dissolveThreshold = t;
        }

        if (state_.deathTimer_ >= params_.deathDuration_) {
            // リスポーン地点の決定
            Vector3 respawnPos = state_.startPosition_;
            const auto& rooms = map.GetRooms();
            if (state_.currentRoomIndex_ >= 0 && state_.currentRoomIndex_ < rooms.size()) {
                const auto& room = rooms[state_.currentRoomIndex_];
                
                // ルーム内の kRoomRespawn を探す
                bool foundRespawn = false;
                for (int y = 0; y < map.GetHeight(); ++y) {
                    for (int x = 0; x < map.GetWidth(); ++x) {
                        if (map.GetChipType(x, y) == MapChip2D::ChipType::kRoomRespawn) {
                            float wx = map.ChipToWorldX(x) + map.GetChipSize() * 0.5f;
                            float wy = map.ChipToWorldY(y) + map.GetChipSize() * 0.5f;
                            
                            // このチップが現在のルーム内にあるか？
                            if (wx >= room.x && wx <= room.x + room.width &&
                                wy >= room.y && wy <= room.y + room.height) {
                                respawnPos = { wx, wy, 0.0f };
                                foundRespawn = true;
                                break;
                            }
                        }
                    }
                    if (foundRespawn) break;
                }
            }

            // 指定地点に復活
            state_.position_ = respawnPos;
            state_.velocity_ = { 0.0f, 0.0f, 0.0f };
            state_.isDead_ = false;
            state_.deathTimer_ = 0.0f;
            state_.isDashing_ = false;
            state_.canDash_ = true;
            state_.wallJumpDirLockTimer_ = 0.0f;
            state_.lockedDirectionX_ = 0.0f;
            
            // 慣性をリセット
            state_.isOnMovingPlatform_ = false;
            state_.platformVelocity_ = { 0.0f, 0.0f, 0.0f };
            state_.recentPlatformVelocity_ = { 0.0f, 0.0f, 0.0f };
            state_.wallPlatformVelocity_ = { 0.0f, 0.0f, 0.0f };
            state_.platformInertiaTimer_ = 0.0f;
            state_.externalVelocityX_ = 0.0f;
            state_.isWallClinging_ = false;
            state_.isWallSliding_ = false;

            // パラメータをリセット
            visuals_.GetPrimitiveObject()->GetMaterial().dissolveThreshold = 0.0f;
            TimeManager::GetInstance().SetTimeScale(1.0f); // スローモーション解除
            
            // リスポーン演出の開始
            state_.isRespawning_ = true;
            state_.respawnTimer_ = 0.0f;
            visuals_.GetPrimitiveObject()->SetScale({ 0.0f, 0.0f, 1.0f });
        }

        // PrimitiveObjectの座標を更新
        visuals_.GetPrimitiveObject()->SetTranslation(state_.position_);
        visuals_.GetPrimitiveObject()->Update();
        return;
    }

    // リスポーン時のスケール拡大演出（この間は操作・物理無効）
    if (state_.isRespawning_) {
        state_.respawnTimer_ += deltaTime;
        float t = (std::min)(state_.respawnTimer_ / params_.respawnDuration_, 1.0f);
        
        // EaseOutBackによる弾むようなポップアップ
        float c1 = 1.70158f;
        float c3 = c1 + 1.0f;
        float p = t - 1.0f;
        float scaleProgress = 1.0f + c3 * (p * p * p) + c1 * (p * p);
        if (scaleProgress < 0.0f) scaleProgress = 0.0f;

        visuals_.GetPrimitiveObject()->SetScale({ params_.halfWidth_ * 2.0f * scaleProgress, params_.halfHeight_ * 2.0f * scaleProgress, 1.0f });

        if (t >= 1.0f) {
            state_.isRespawning_ = false;
            visuals_.GetPrimitiveObject()->SetScale({ params_.halfWidth_ * 2.0f, params_.halfHeight_ * 2.0f, 1.0f });
        }

        visuals_.GetPrimitiveObject()->GetMaterial().color = params_.colorNormal_; // リスポーン中は通常色
        visuals_.GetPrimitiveObject()->SetTranslation(state_.position_);
        visuals_.GetPrimitiveObject()->Update();
        return; // ここでリターンして通常のゲームロジックをスキップ
    }

    // カメラスライド（ルーム遷移）中の硬直処理
    if (isTransitioning) {
        // 遷移中は操作も物理挙動（重力など）も行わず、時間を止める。
        // ただし、遷移前の速度（state_.velocity_）は保持しておくことで、遷移完了後にジャンプの勢いなどをそのまま引き継ぐ。
        
        // アニメーション等の描画だけは更新する
        visuals_.GetPrimitiveObject()->SetTranslation(state_.position_);
        visuals_.GetPrimitiveObject()->Update();
        return;
    }

    // 壁ジャンプタイマーの更新
    if (state_.wallJumpTimer_ > 0.0f) {
        state_.wallJumpTimer_ -= deltaTime;
    }

    // 入力処理
    physics_.Update(state_, params_, currentInput_, visuals_, deltaTime, this);

    // 現在のルームを特定する
    const auto& rooms = map.GetRooms();
    bool isInAnyRoom = false;
    for (int i = 0; i < (int)rooms.size(); ++i) {
        if (state_.position_.x >= rooms[i].x && state_.position_.x <= rooms[i].x + rooms[i].width &&
            state_.position_.y >= rooms[i].y && state_.position_.y <= rooms[i].y + rooms[i].height) {
            state_.currentRoomIndex_ = i;
            isInAnyRoom = true;
            break;
        }
    }

    // 完全にルームから逸脱している場合は死亡する
    // ただし、トランジション中は isTransitioning = true でUpdateWithMapの先頭で早期リターンされるため、ここには来ない。
    // また、roomsが設定されていない（空）の場合は無視する。
    if (!rooms.empty() && !isInAnyRoom && !state_.isDead_) {
        Kill(true);
    }

    float deathY = -10.0f;
    if (state_.currentRoomIndex_ >= 0 && state_.currentRoomIndex_ < (int)rooms.size()) {
        // ルームの下端から少し余裕をもたせた高さをデスマッチラインとする
        deathY = rooms[state_.currentRoomIndex_].y - 2.0f;
    }

    // 画面外落下時のリスポーン演出移行
    if (state_.position_.y < deathY) {
        Kill(true);
    }

    // 色の更新
    visuals_.GetPrimitiveObject()->GetMaterial().color = (state_.isDashing_ || !state_.canDash_) ? params_.colorDashed_ : params_.colorNormal_;

    // 走りエフェクトの発生
    if (state_.isOnGround_ && std::abs(state_.velocity_.x) > 0.1f) {
        state_.runDustTimer_ += deltaTime;
        if (state_.runDustTimer_ >= params_.runDustInterval_) {
            state_.runDustTimer_ = 0.0f;
            float dirX = (state_.velocity_.x > 0.0f) ? 1.0f : -1.0f;
            // プレイヤーの後ろの足元から砂埃を出す
            visuals_.SpawnRunDust({state_.position_.x - dirX * params_.halfWidth_, state_.position_.y - params_.halfHeight_, 0.0f}, dirX);
        }
    } else {
        state_.runDustTimer_ = 0.0f;
    }

    // 砂埃パーティクルの更新

    // --- バグ検知処理（リプレイ再生中は無効化） ---
    if (!ReplayManager::GetInstance()->IsPlaying()) {
        // 1. 亜空間への落下、または座標の破綻
        if (state_.position_.y < (deathY - 50.0f) || std::isnan(state_.position_.x) || std::isnan(state_.position_.y)) {
            ReplayManager::GetInstance()->TriggerBugReport("プレイヤーの座標が破綻、またはマップ外に落下しました。");
            // 安全処理
            state_.position_ = state_.startPosition_;
            state_.velocity_ = { 0.0f, 0.0f, 0.0f };
            state_.isDead_ = true;
        }
        
        // 2. スタック検知（入力があるのに動いていない）
        if ((std::abs(state_.velocity_.x) > 0.1f || std::abs(state_.velocity_.y) > 0.1f) && 
            std::abs(state_.position_.x - state_.prevPositionForBugCheck_.x) < 0.001f && 
            std::abs(state_.position_.y - state_.prevPositionForBugCheck_.y) < 0.001f) {
            
            state_.stuckTimer_ += deltaTime;
            if (state_.stuckTimer_ > 2.0f) { // 2秒間スタック
                ReplayManager::GetInstance()->TriggerBugReport("2秒間移動が反映されないスタック状態を検知しました。");
                state_.stuckTimer_ = 0.0f;
                state_.isDead_ = true; // スタック脱出のために死亡扱いにする
            }
        } else {
            state_.stuckTimer_ = 0.0f;
        }
    } else {
        state_.stuckTimer_ = 0.0f;
    }
    
    state_.prevPositionForBugCheck_ = state_.position_;
    // --------------------

    // 色の更新
    visuals_.GetPrimitiveObject()->GetMaterial().color = state_.canDash_ ? params_.colorNormal_ : params_.colorDashed_;

    // PrimitiveObjectの座標を更新
    visuals_.GetPrimitiveObject()->SetTranslation(state_.position_);
    visuals_.GetPrimitiveObject()->Update();
    
    // アイテム（コインなど）との当たり判定は physics_.Update() 内で行われるため削除

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
            if (sizeChanged && visuals_.GetPrimitiveObject()) {
                visuals_.GetPrimitiveObject()->SetScale({ params_.halfWidth_ * 2.0f, params_.halfHeight_ * 2.0f, 1.0f });
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
    
    if (visuals_.GetPrimitiveObject()) {
        visuals_.GetPrimitiveObject()->SetTranslation(state_.position_);
        visuals_.GetPrimitiveObject()->GetMaterial().color = params_.colorNormal_;
        visuals_.GetPrimitiveObject()->SetScale({ params_.halfWidth_ * 2.0f, params_.halfHeight_ * 2.0f, 1.0f });
        visuals_.GetPrimitiveObject()->SetRotation({ 0.0f, 0.0f, 0.0f });
        visuals_.GetPrimitiveObject()->Update();
    }
    visuals_.ClearEffects();
}






AABB2D Player2D::GetAABB() const {
    return physics_.GetAABB(state_, params_);
}
void Player2D::Update() {
    // IComponentとしてのUpdateは現在使用せず、UpdateWithMapを使用する
}
