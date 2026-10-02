#include "DashRecovery.h"
#include "BlockFactory.h"
#include "Game2D/Player/Player2D.h"
#include "Graphics/GameCamera.h"
#include "Graphics/CameraManager.h"
#include "Resource/Model/ModelManager.h"
#include "Renderer/DirectXCommon/DirectXCommon.h"
#include "Effect/ParticleCommon.h"
#include "Effect/GPUParticle/GPUParticleSystem.h"
#include "Core/Utility/TransformFunctions.h"
#include "Core/TimeManager.h"
#include <cmath>
#include <format>
#ifdef USE_IMGUI
#include "Editor/EditorManager.h"
#include <imgui.h>
#endif
#include "Editor/Replay/ReplayManager.h"

// BlockFactoryへの自動登録マクロ（プロジェクト起動時に登録されます）
REGISTER_BLOCK_CLASS(DashRecovery);

float DashRecovery::sGlobalCollectCooldown_ = 0.0f;
int DashRecovery::sTotalCollectCount_ = 0;

void DashRecovery::Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) {
    basePosition_ = { worldX, worldY, 0.0f };
    baseScale_ = { width, height, 1.0f };
    isActive_ = true;
    respawnTimer_ = 0.0f;
    respawnAnimTimer_ = 0.0f;
    hoverTimer_ = 0.0f;

    // 本体 GameObject の生成
    gameObject_ = std::make_unique<GameObject>("DashRecovery");
    auto* tc = gameObject_->AddComponent<TransformComponent>();
    auto* prc = gameObject_->AddComponent<PrimitiveRendererComponent>();

    prc->Initialize(device, boxPrimitive);
    prc->GetMaterial().color = color_;
    tc->SetScale(baseScale_);
    tc->SetPosition(basePosition_);
    prc->GetMaterial().lightingType = 1;

    SetupCollider();

    // GPUパーティクルエフェクトの初期化 (DashRecaveryEffect)
    gpuParticleSystem_ = std::make_unique<GPUParticleSystem>();
    gpuParticleSystem_->Initialize(device);
    const std::string particlePath = "resources/json/shared/Particle/DashRecaveryEffect.json";
    if (gpuParticleSystem_->LoadFromFile(particlePath)) {
        // アイテム取得ワンショット再生用に調整
        auto& pData = gpuParticleSystem_->GetData();
        pData.isLoop = false;
        pData.duration = 1.2f; // パーティクル生存時間(1.0s)をカバー
    }
}

void DashRecovery::Update() {
    float deltaTime = TimeManager::GetInstance().GetDeltaTime();

    // グローバルクールダウンの更新
    if (sGlobalCollectCooldown_ > 0.0f) {
        sGlobalCollectCooldown_ -= deltaTime;
    }

    bool isPlayingOrReplaying = false;
#ifdef USE_IMGUI
    if (EditorManager::IsPlaying()) {
        isPlayingOrReplaying = true;
    }
#else
    isPlayingOrReplaying = true;
#endif
    if (ReplayManager::GetInstance()->IsPlaying()) {
        isPlayingOrReplaying = true;
    }

    bool isAnimActive = isPlayingOrReplaying && !ReplayManager::GetInstance()->IsPaused();
    float animDeltaTime = isAnimActive ? deltaTime : 0.0f;

    if (isActive_) {
        hoverTimer_ += animDeltaTime;
        // 上下にふわふわ浮遊
        float offsetY = std::sin(hoverTimer_ * hoverSpeed_) * hoverAmplitude_;
        Vector3 currentPos = { basePosition_.x, basePosition_.y + offsetY, basePosition_.z };

        // ゆっくりY軸回転
        float rotY = hoverTimer_ * rotationSpeed_;

        // 復活時のポップアップバウンドアニメーション
        Vector3 currentScale = baseScale_;
        if (respawnAnimTimer_ > 0.0f) {
            respawnAnimTimer_ -= animDeltaTime;
            float t = 1.0f - (respawnAnimTimer_ / respawnAnimDuration_);
            // 0 -> 1.25 -> 1.0 のバウンド拡縮
            float scalePop = 1.0f + 0.25f * std::sin(t * 3.14159265f);
            currentScale.x *= scalePop;
            currentScale.y *= scalePop;
        }

        if (gameObject_) {
            if (auto* tc = gameObject_->GetComponent<TransformComponent>()) {
                tc->SetPosition(currentPos);
                tc->SetRotation({ 0.0f, rotY, 0.0f });
                tc->SetScale(currentScale);
            }
            if (auto* prc = gameObject_->GetComponent<PrimitiveRendererComponent>()) {
                prc->GetMaterial().color = color_;
            }
        }
    } else {
        // 非アクティブ（クールダウン中）：タイマー更新
        if (respawnTime_ > 0.0f) {
            respawnTimer_ += animDeltaTime;
            if (respawnTimer_ >= respawnTime_) {
                Respawn();
            }
        }

        // クールダウン中は半透明・縮小のゴースト表示
        if (gameObject_) {
            if (auto* tc = gameObject_->GetComponent<TransformComponent>()) {
                tc->SetPosition(basePosition_);
                tc->SetRotation({ 0.0f, 0.0f, 0.0f });
                tc->SetScale({ baseScale_.x * 0.5f, baseScale_.y * 0.5f, baseScale_.z * 0.5f });
            }
            if (auto* prc = gameObject_->GetComponent<PrimitiveRendererComponent>()) {
                Vector4 ghostColor = color_;
                ghostColor.w = inactiveAlpha_;
                prc->GetMaterial().color = ghostColor;
            }
        }
    }

    if (gameObject_) {
        gameObject_->Update();
    }

    // GPUパーティクルの更新
    if (gpuParticleSystem_ && (gpuParticleSystem_->IsPlaying() || gpuParticleSystem_->GetTotalActiveParticles() > 0)) {
        gpuParticleSystem_->Update(animDeltaTime);
    }
}

void DashRecovery::Draw() {
    if (gameObject_) {
        gameObject_->Draw();
    }

    // GPUパーティクルの描画
    if (gpuParticleSystem_ && (gpuParticleSystem_->IsPlaying() || gpuParticleSystem_->GetTotalActiveParticles() > 0)) {
        ParticleCommon* particleCommon = ParticleCommon::GetInstance();
        if (particleCommon) {
            particleCommon->PreDraw();
            auto commandList = DirectXCommon::GetInstance()->GetCommandList();
            auto cameraMgr = CameraManager::GetInstance();
            Matrix4x4 viewProj = TransformFunctions::Multiply(cameraMgr->GetViewMatrix(), cameraMgr->GetProjectionMatrix());
            Matrix4x4 camMat = TransformFunctions::Inverse(cameraMgr->GetViewMatrix());
            ModelManager* modelMgr = ModelManager::GetInstance();
            gpuParticleSystem_->Draw(commandList, viewProj, camMat, particleCommon, modelMgr);
        }
    }
}

void DashRecovery::OnCollision(Player2D* player) {
    if (!isActive_ || !player) return;

    // プレイヤーが接触した瞬間にダッシュを回復
    Collect(player);
}

void DashRecovery::OnPlayerStand() {
    // 固体ブロック設定時にプレイヤーが乗った場合（OnCollisionで処理されるため補足用）
}

void DashRecovery::OnPlayerTouch() {
    // 固体ブロック設定時にプレイヤーが横・下から触れた場合
}

void DashRecovery::Collect(Player2D* player) {
    if (!isActive_ || !player) return;

    // 連続取得防止ガード：
    // 直前（0.25秒以内）に別のDashRecoveryを取得しており、かつプレイヤーがまだダッシュを使っていない（canDash_ == true かつ !isDashing_）場合、
    // 隣接する回復ブロックの誤爆や多重爆発を防ぐため、このブロックは消費せず温存する
    const auto& pState = player->GetState();
    if (sGlobalCollectCooldown_ > 0.0f && pState.canDash_ && !pState.isDashing_) {
        return; // 温存
    }

    sGlobalCollectCooldown_ = 0.25f; // 0.25秒間の連続取得防止ガード
    sTotalCollectCount_++;

    OutputDebugStringA(std::format("[DashRecovery] Collected! Chip({}, {}), TotalCount: {}\n",
        chipX_, chipY_, sTotalCollectCount_).c_str());

    // 1. ダッシュ回数を即座に全回復
    player->RefillDash();

    // 2. 壁登りスタミナも全回復
    auto& state = player->GetState();
    state.stamina_ = player->GetParams().maxStamina_;
    state.isExhausted_ = false;

    // 3. 取得時の爽快感・手応えを与えるヒットストップ
    if (hitstopDuration_ > 0.0f) {
        player->ApplyHitstop(hitstopDuration_);
    }

    // 4. 微細なカメラシェイク
    if (player->GetCamera() && cameraShakePower_ > 0.0f) {
        player->GetCamera()->Shake(cameraShakePower_, 0.1f);
    }

    // 5. 状態を非アクティブ（クールダウン）に移行
    isActive_ = false;
    respawnTimer_ = 0.0f;

    // 6. コライダーを一時的に無効化
    if (gameObject_) {
        if (auto* cc = gameObject_->GetComponent<ColliderComponent>()) {
            cc->SetLayerMask(0);
            cc->SetIsSolid(false);
        }
    }

    // 7. GPUパーティクルエフェクト (DashRecaveryEffect) の再生
    if (gpuParticleSystem_) {
        Vector3 effectPos = basePosition_;
        if (gameObject_) {
            if (auto* tc = gameObject_->GetComponent<TransformComponent>()) {
                effectPos = tc->GetPosition();
            }
        }
        gpuParticleSystem_->SetPosition(effectPos);
        gpuParticleSystem_->Restart();
        if (useBurstTrigger_) {
            gpuParticleSystem_->TriggerBurstAll();
        }
        gpuParticleSystem_->Play();
    }
}

void DashRecovery::Respawn() {
    isActive_ = true;
    respawnTimer_ = 0.0f;
    respawnAnimTimer_ = respawnAnimDuration_;

    if (gameObject_) {
        if (auto* cc = gameObject_->GetComponent<ColliderComponent>()) {
            cc->SetLayerMask(kLayerBlock);
            cc->SetIsSolid(IsSolid());
        }
        if (auto* prc = gameObject_->GetComponent<PrimitiveRendererComponent>()) {
            prc->GetMaterial().color = color_;
        }
    }
}

void DashRecovery::SetProperties(const nlohmann::json& properties) {
    if (properties.contains("respawnTime") && properties["respawnTime"].is_number()) {
        respawnTime_ = properties["respawnTime"].get<float>();
    }
    if (properties.contains("isSolid") && properties["isSolid"].is_boolean()) {
        isSolid_ = properties["isSolid"].get<bool>();
        if (gameObject_) {
            if (auto* cc = gameObject_->GetComponent<ColliderComponent>()) {
                cc->SetIsSolid(IsSolid());
            }
        }
    }
    if (properties.contains("hitstopDuration") && properties["hitstopDuration"].is_number()) {
        hitstopDuration_ = properties["hitstopDuration"].get<float>();
    }
    if (properties.contains("cameraShakePower") && properties["cameraShakePower"].is_number()) {
        cameraShakePower_ = properties["cameraShakePower"].get<float>();
    }
    if (properties.contains("useBurstTrigger") && properties["useBurstTrigger"].is_boolean()) {
        useBurstTrigger_ = properties["useBurstTrigger"].get<bool>();
    }
    if (properties.contains("color") && properties["color"].is_array() && properties["color"].size() >= 4) {
        color_.x = properties["color"][0].get<float>();
        color_.y = properties["color"][1].get<float>();
        color_.z = properties["color"][2].get<float>();
        color_.w = properties["color"][3].get<float>();
    }
}

void DashRecovery::Reset() {
    BaseBlock::Reset();
    isActive_ = true;
    respawnTimer_ = 0.0f;
    respawnAnimTimer_ = 0.0f;
    hoverTimer_ = 0.0f;

    if (gpuParticleSystem_) {
        gpuParticleSystem_->Restart();
        gpuParticleSystem_->Pause();
    }

    if (gameObject_) {
        if (auto* tc = gameObject_->GetComponent<TransformComponent>()) {
            tc->SetPosition(basePosition_);
            tc->SetScale(baseScale_);
            tc->SetRotation({ 0.0f, 0.0f, 0.0f });
        }
        if (auto* prc = gameObject_->GetComponent<PrimitiveRendererComponent>()) {
            prc->GetMaterial().color = color_;
        }
        if (auto* cc = gameObject_->GetComponent<ColliderComponent>()) {
            cc->SetLayerMask(kLayerBlock);
            cc->SetIsSolid(IsSolid());
        }
    }
}

#ifdef USE_IMGUI
void DashRecovery::DrawImGui() {
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "[%s ロジック調整]", "DashRecovery");
    if (ImGui::Checkbox("固体（足場）にする", &isSolid_)) {
        if (gameObject_) {
            if (auto* cc = gameObject_->GetComponent<ColliderComponent>()) {
                cc->SetIsSolid(IsSolid());
            }
        }
    }
    ImGui::Checkbox("バースト一斉放出モード (Burst)", &useBurstTrigger_);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("OFF: エディターの[>]再生と同じ自然な粒子量 (約8個)\nON: burst設定の一斉放出 (計25個)");
    }
    ImGui::DragFloat("復活時間 (秒)", &respawnTime_, 0.1f, 0.1f, 10.0f);
    ImGui::DragFloat("ヒットストップ時間", &hitstopDuration_, 0.01f, 0.0f, 0.2f);
    ImGui::DragFloat("カメラシェイク強度", &cameraShakePower_, 0.05f, 0.0f, 2.0f);
    ImGui::DragFloat("浮遊速度", &hoverSpeed_, 0.1f, 0.0f, 10.0f);
    ImGui::DragFloat("浮遊幅", &hoverAmplitude_, 0.01f, 0.0f, 1.0f);
    ImGui::ColorEdit4("クリスタルカラー", &color_.x);

    ImGui::Separator();
    ImGui::Text("状態: %s", isActive_ ? "待機中 (取得可能)" : "クールダウン中");
    if (!isActive_ && respawnTime_ > 0.0f) {
        ImGui::ProgressBar(respawnTimer_ / respawnTime_, ImVec2(0.0f, 0.0f));
    }

    ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.4f, 1.0f), "累計取得発動数: %d", sTotalCollectCount_);

    if (gpuParticleSystem_) {
        ImGui::Text("GPUパーティクル生存数: %u / %u", 
            gpuParticleSystem_->GetTotalActiveParticles(), 
            gpuParticleSystem_->GetTotalMaxParticles());
        if (ImGui::Button("エフェクト再生テスト (DashRecaveryEffect)")) {
            Vector3 testPos = basePosition_;
            if (gameObject_) {
                if (auto* tc = gameObject_->GetComponent<TransformComponent>()) {
                    testPos = tc->GetPosition();
                }
            }
            gpuParticleSystem_->SetPosition(testPos);
            gpuParticleSystem_->Restart();
            if (useBurstTrigger_) gpuParticleSystem_->TriggerBurstAll();
            gpuParticleSystem_->Play();
        }
    }

    if (ImGui::Button("即時復活 (Respawn)")) {
        Respawn();
    }
    ImGui::SameLine();
    if (ImGui::Button("状態リセット (Reset)")) {
        Reset();
    }
}
#endif
