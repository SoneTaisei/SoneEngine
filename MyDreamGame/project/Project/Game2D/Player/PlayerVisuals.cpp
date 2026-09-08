#include "Graphics/TextureManager.h"
#include "PlayerVisuals.h"
#include "Core/TimeManager.h"
#include "Renderer/Renderer.h"
#include "Resource/Primitive/PrimitiveManager.h"
#include <random>
#include <cmath>

#include "Core/Utility/LogManager.h"
#ifdef USE_IMGUI
#include "Editor/EditorManager.h"
#endif

void PlayerVisuals::ReloadAnimations() {
    if (!animator_) return;

    animator_->ClearJointOverrides();

    // 新プレイヤーモデル用のアニメーション読み込み（エディタで保存したJSONを優先読み込み）
    if (LoadAnimationFromJsonFile(idleAnimation_, "resources/json/shared/Player/Idle_animation.json")) {
        std::string loadedBones = "";
        for (const auto& [nodeName, nodeData] : idleAnimation_.nodeAnimations) {
            if (!nodeData.translate.empty() || !nodeData.rotate.empty() || !nodeData.scale.empty()) {
                if (!loadedBones.empty()) loadedBones += ", ";
                loadedBones += nodeName;
            }
        }
        LogManager::GetInstance()->AddLog(LogLevel::Info, "[PlayerVisuals] Loaded Idle_animation.json duration: " + std::to_string(idleAnimation_.duration) + "s, animated bones: [" + loadedBones + "]");
    } else {
        idleAnimation_ = LoadAnimationFile("resources/Object/Original/player", "Player.gltf", "Idle");
        LogManager::GetInstance()->AddLog(LogLevel::Info, "[PlayerVisuals] Fallback to glTF Idle animation.");
    }

    if (LoadAnimationFromJsonFile(walkAnimation_, "resources/json/shared/Player/walk_animation.json")) {
        std::string loadedBones = "";
        for (const auto& [nodeName, nodeData] : walkAnimation_.nodeAnimations) {
            if (!nodeData.translate.empty() || !nodeData.rotate.empty() || !nodeData.scale.empty()) {
                if (!loadedBones.empty()) loadedBones += ", ";
                loadedBones += nodeName;
            }
        }
        LogManager::GetInstance()->AddLog(LogLevel::Info, "[PlayerVisuals] Loaded walk_animation.json duration: " + std::to_string(walkAnimation_.duration) + "s, animated bones: [" + loadedBones + "]");
    } else {
        walkAnimation_ = idleAnimation_;
    }
    if (!LoadAnimationFromJsonFile(jumpAnimation_, "resources/json/shared/Player/jump_animation.json")) {
        jumpAnimation_ = idleAnimation_;
    }

    if (!LoadAnimationFromJsonFile(wallClimbAnimation_, "resources/json/shared/Player/wall_climb_animation.json")) {
        wallClimbAnimation_ = idleAnimation_;
    }
    if (!LoadAnimationFromJsonFile(holdingWallAnimation_, "resources/json/shared/Player/holding_wall.json")) {
        holdingWallAnimation_ = idleAnimation_;
    }
    if (!LoadAnimationFromJsonFile(airDashAnimation_, "resources/json/shared/Player/air_dash_animation.json")) {
        airDashAnimation_ = idleAnimation_;
    }
    if (!LoadAnimationFromJsonFile(swingAnimation_, "resources/json/shared/Player/swing_animation.json")) {
        swingAnimation_ = idleAnimation_;
    }

    if (currentAnimType_ == PlayerAnimType::Idle || currentAnimType_ == PlayerAnimType::None) {
        animator_->SetAnimation(idleAnimation_);
        animator_->Play();
    }
}

void PlayerVisuals::Initialize(ID3D12Device* device, Primitive* boxPrimitive, Primitive* ringPrimitive, uint32_t texHandle, Model* playerModel) {
    primitiveObj_ = std::make_unique<PrimitiveObject>();
    primitiveObj_->Initialize(device, boxPrimitive);
    primitiveObj_->SetName("Player");
    primitiveObj_->SetTextureHandle(TextureManager::GetInstance()->GetGpuHandle(texHandle));
    primitiveObj_->GetMaterial().lightingType = 1;

    if (playerModel) {
        modelObj_ = std::make_unique<Object3D>();
        modelObj_->Initialize(device, playerModel);
        modelObj_->SetName("Player3DModel");
        modelObj_->GetMaterial().lightingType = 1;

        animator_ = std::make_unique<AnimatorComponent>();
        animator_->Initialize();
        animator_->SetModelData(playerModel->GetModelData());

        ReloadAnimations();

        capePhysics_.Initialize(animator_.get());

        modelObj_->SetAnimator(animator_.get());
    }

    dashRingPrimitive_ = std::make_unique<PrimitiveObject>();
    dashRingPrimitive_->Initialize(device, ringPrimitive);
    dashRingPrimitive_->SetName("DashRing");
    dashRingPrimitive_->SetTextureHandle(TextureManager::GetInstance()->GetGpuHandle(texHandle));
    dashRingPrimitive_->GetMaterial().lightingType = 0;
    dashRingPrimitive_->GetMaterial().enableEnvironmentMap = 0;
    dashRingPrimitive_->SetIsBillboard(false);
    dashRingPrimitive_->SetIsDoubleSided(true);
    dashRingPrimitive_->SetBlendMode(BlendMode::kBlendModeAdd);

    dustPrimitive_ = std::make_unique<PrimitiveObject>();
    dustPrimitive_->Initialize(device, boxPrimitive);
    dustPrimitive_->SetName("Dust");
    dustPrimitive_->GetMaterial().lightingType = 0;
    dustPrimitive_->GetMaterial().color = { 0.8f, 0.8f, 0.8f, 0.8f }; // 白っぽい砂ぼこり

    confettiPrimitive_ = std::make_unique<PrimitiveObject>();
    confettiPrimitive_->Initialize(device, boxPrimitive);
    confettiPrimitive_->SetName("Confetti");
    confettiPrimitive_->GetMaterial().lightingType = 0;

    smokePrimitive_ = std::make_unique<PrimitiveObject>();
    smokePrimitive_->Initialize(device, boxPrimitive);
    smokePrimitive_->SetName("SmokeBomb");
    uint32_t smokeTex = TextureManager::GetInstance()->Load("resources/Sprite/Original/smoke.png");
    smokePrimitive_->SetTextureHandle(TextureManager::GetInstance()->GetGpuHandle(smokeTex));
    smokePrimitive_->GetMaterial().color = { 1.0f, 1.0f, 1.0f, 1.0f };
    smokePrimitive_->GetMaterial().lightingType = 0;
    smokePrimitive_->SetIsBillboard(true);
    smokePrimitive_->SetIsDoubleSided(true);
    smokePrimitive_->SetBlendMode(BlendMode::kBlendModeNormal); // 通常αブレンド（両面描画で美しく透過）
}

void PlayerVisuals::Update(const PlayerState& state, const PlayerParams& params, float deltaTime) {
#ifdef USE_IMGUI
    static bool s_wasPlaying = false;
    bool isNowPlaying = EditorManager::IsPlaying();
    if (isNowPlaying && !s_wasPlaying) {
        ReloadAnimations();
    }
    s_wasPlaying = isNowPlaying;
#endif

    visualTime_ += deltaTime;
    
    if (primitiveObj_) {
        primitiveObj_->SetTranslation(state.position_);
        
        if (state.isDashing_) {
            primitiveObj_->GetMaterial().color = params.colorDashed_;
            // ダッシュ中は少し細長くする
            Vector3 dashDir = state.velocity_;
            dashDir.z = 0.0f;
            float speed = 1.0f;
            if (dashDir.x != 0.0f || dashDir.y != 0.0f) {
                float length = std::sqrt(dashDir.x * dashDir.x + dashDir.y * dashDir.y);
                if (length > 0.0f) { dashDir.x /= length; dashDir.y /= length; speed = length; }
            }
            float stretch = 1.0f + (speed * 0.02f);
            float squash = 1.0f / stretch;
            primitiveObj_->SetScale({ params.halfWidth_ * 2.0f * stretch, params.halfHeight_ * 2.0f * squash, 1.0f });
            if (dashDir.x != 0.0f || dashDir.y != 0.0f) {
                // ダッシュ時のプレイヤー本体の回転は無効化する（エフェクト等はそのまま）
                primitiveObj_->SetRotation({ 0.0f, 0.0f, 0.0f });
            }
        } else {
            if (state.stamina_ <= params.maxStamina_ * 0.2f || state.isExhausted_) {
                float blink = std::sin(visualTime_ * 40.0f);
                if (blink > 0.0f) {
                    primitiveObj_->GetMaterial().color = params.colorTired_;
                } else {
                    primitiveObj_->GetMaterial().color = { 1.0f, 1.0f, 1.0f, 1.0f };
                }
            } else if (state.stamina_ <= params.maxStamina_ * 0.5f) {
                float blink = std::sin(visualTime_ * 20.0f);
                if (blink > 0.0f) {
                    primitiveObj_->GetMaterial().color = params.colorTired_;
                } else {
                    primitiveObj_->GetMaterial().color = params.colorNormal_;
                }
            } else {
                primitiveObj_->GetMaterial().color = params.colorNormal_;
            }
            primitiveObj_->SetScale({ params.halfWidth_ * 2.0f, params.halfHeight_ * 2.0f, 1.0f });
            primitiveObj_->SetRotation({ 0.0f, 0.0f, 0.0f });
        }
        
        if (!state.isDead_) {
            primitiveObj_->Update();
        }
    }

    if (modelObj_) {
        if (animator_) {
            if (state.isDashing_) {
                // 空中ダッシュアニメーションの再生
                if (currentAnimType_ != PlayerAnimType::AirDash) {
                    currentAnimType_ = PlayerAnimType::AirDash;
                    airDashAnimTime_ = 0.0f;
                }
                airDashAnimTime_ = AdvanceAnimationTime(airDashAnimTime_, airDashAnimation_.duration, deltaTime, AnimationWrapMode::Loop);
                animator_->ClearJointOverrides();
                animator_->SetAnimation(airDashAnimation_);
                animator_->SetTime(airDashAnimTime_);
                animator_->Stop(); // 手動で時間を制御するため自動更新を停止
            } else if (state.isSwingingChain_) {
                airDashAnimTime_ = 0.0f;
                climbBlendFactor_ = 0.0f;
                wallClimbAnimTime_ = 0.0f;
                holdingWallAnimTime_ = 0.0f;

                if (currentAnimType_ != PlayerAnimType::Swing) {
                    currentAnimType_ = PlayerAnimType::Swing;
                }

                // 体・足などのベースポーズとして正面構えアニメーションを適用
                animator_->SetAnimation(swingAnimation_);
                animator_->SetTime(0.0f);
                animator_->Stop();
                animator_->ClearJointOverrides();

                // 振り子の物理シミュレーション角度（真下=0、+で右/反時計回り）から手の位置・回転をプログラムで直接計算
                float theta = state.chainSwingTheta_;
                float sinT = std::sin(theta);
                float cosT = std::cos(theta);

                // 手の位置：プレイヤーは正面（カメラ向き）のため、ワールド右(+X)はモデルのローカル左(-X)に対応
                // 振り子の物理ベクトル (sinθ, -cosθ) に画面上で完全に一致するよう handX は -sinT とする
                float swingRadius = 0.16f;
                float handX = -swingRadius * sinT;
                float handY = 0.48f - swingRadius * cosT;
                float handZ = 0.22f + 0.05f * cosT; // 下の時は遠く（前）、上の時は少し手前

                Vector3 handPos = { handX, handY, handZ };

                // 腕の回転：振り子の方向に向くダイナミックな傾き
                float rPitch = -0.90f + 0.35f * cosT;
                float rYaw   =  0.15f - 0.30f * sinT;
                float rRoll  =  0.25f - 0.25f * sinT;
                Quaternion rRot = MakeEulerQuat(rPitch, rYaw, rRoll);
                Quaternion lRot = { rRot.x, -rRot.y, -rRot.z, rRot.w };

                // 手だけプログラムで完全同期オーバーライド
                animator_->SetJointTranslationOverride("右手", handPos, 1.0f);
                animator_->SetJointRotationOverride("右手", rRot, 1.0f);
                animator_->SetJointTranslationOverride("左手", handPos, 1.0f);
                animator_->SetJointRotationOverride("左手", lRot, 1.0f);

                // 体幹も振り子の遠心力・引っ張られる力に合わせて少し踏ん張る
                float bPitch = 0.15f - 0.05f * cosT;
                float bYaw   = -0.04f * sinT;
                float bRoll  =  0.06f * sinT;
                Quaternion bRot = MakeEulerQuat(bPitch, bYaw, bRoll);
                animator_->SetJointRotationOverride("体", bRot, 1.0f);

                // 頭は常に正面（カメラ目線）をキープするように相殺
                Quaternion hRot = MakeEulerQuat(-bPitch, 0.0f, -bRoll);
                animator_->SetJointRotationOverride("頭", hRot, 1.0f);
            } else if (state.isHoldingChain_) {
                airDashAnimTime_ = 0.0f;
                climbBlendFactor_ = 0.0f;
                wallClimbAnimTime_ = 0.0f;
                holdingWallAnimTime_ = 0.0f;

                currentAnimType_ = PlayerAnimType::Hold;

                // ベースポーズとして正面構えアニメーションを適用
                animator_->SetAnimation(swingAnimation_);
                animator_->SetTime(0.0f);
                animator_->Stop();
                animator_->ClearJointOverrides();

                // 両手で宝石を手前（胸の前）で持つポーズ
                // 胸元中心: (0.0f, 0.44f, 0.22f)。両手で宝石を大切そうに抱え込む
                Vector3 rHandPos = {  0.07f, 0.44f, 0.22f };
                Vector3 lHandPos = { -0.07f, 0.44f, 0.22f };

                // 腕を前に出して内側に向け、両手で挟み込む自然な回転
                Quaternion rRot = MakeEulerQuat(-0.85f,  0.25f,  0.35f);
                Quaternion lRot = MakeEulerQuat(-0.85f, -0.25f, -0.35f);

                animator_->SetJointTranslationOverride("右手", rHandPos, 1.0f);
                animator_->SetJointRotationOverride("右手", rRot, 1.0f);
                animator_->SetJointTranslationOverride("左手", lHandPos, 1.0f);
                animator_->SetJointRotationOverride("左手", lRot, 1.0f);

                // 体幹：やや胸を張る自然な立ち姿勢
                Quaternion bRot = MakeEulerQuat(0.08f, 0.0f, 0.0f);
                animator_->SetJointRotationOverride("体", bRot, 1.0f);

                // 頭：手元の宝石を優しく見つめるよう少し下を向く
                Quaternion hRot = MakeEulerQuat(0.12f, 0.0f, 0.0f);
                animator_->SetJointRotationOverride("頭", hRot, 1.0f);
            } else {
                swingAnimTime_ = 0.0f;
                airDashAnimTime_ = 0.0f;

                // しがみつき中ならブレンド率を上げ、それ以外は下げる
                bool isClinging = state.isWallClinging_ || state.isWallSliding_;
                if (isClinging) {
                    climbBlendFactor_ += deltaTime * 5.0f; // 約0.2秒で最大値1.0fへ遷移
                    if (climbBlendFactor_ > 1.0f) climbBlendFactor_ = 1.0f;

                    // 壁つかまり移動（登り・降り）のアニメーション判定
                    bool isClimbMoving = (std::abs(state.velocity_.y) > 0.1f);
                    if (isClimbMoving) {
                        if (currentAnimType_ != PlayerAnimType::WallClimb) {
                            currentAnimType_ = PlayerAnimType::WallClimb;
                            wallClimbAnimTime_ = 0.0f;
                        }
                        // 上下移動に合わせてアニメーション時間を進行
                        float speedFactor = std::clamp(std::abs(state.velocity_.y) / 5.0f, 0.5f, 2.0f);
                        wallClimbAnimTime_ = AdvanceAnimationTime(wallClimbAnimTime_, wallClimbAnimation_.duration, deltaTime * speedFactor, AnimationWrapMode::Loop);
                        animator_->ClearJointOverrides();
                        animator_->SetAnimation(wallClimbAnimation_);
                        animator_->SetTime(wallClimbAnimTime_);
                        animator_->Stop(); // 手動で時間を制御
                    } else {
                        if (currentAnimType_ != PlayerAnimType::HoldingWall) {
                            currentAnimType_ = PlayerAnimType::HoldingWall;
                            holdingWallAnimTime_ = 0.0f;
                        }
                        // 静止した崖つかまり・壁つかまり時は holding_wall アニメーションを再生（最後のフレームで停止）
                        holdingWallAnimTime_ = AdvanceAnimationTime(holdingWallAnimTime_, holdingWallAnimation_.duration, deltaTime, AnimationWrapMode::HoldLastFrame);
                        animator_->ClearJointOverrides();
                        animator_->SetAnimation(holdingWallAnimation_);
                        animator_->SetTime(holdingWallAnimTime_);
                        animator_->Stop(); // 手動で時間を制御（最後のフレームの姿勢を維持）
                    }
                } else {
                    climbBlendFactor_ -= deltaTime * 5.0f;
                    if (climbBlendFactor_ < 0.0f) climbBlendFactor_ = 0.0f;
                    wallClimbAnimTime_ = 0.0f;
                    holdingWallAnimTime_ = 0.0f;

                    animator_->ClearJointOverrides();
                    animator_->Play(); // 通常アニメーションは自動再生
                    animator_->SetWrapMode(AnimationWrapMode::Loop);

                    PlayerAnimType targetType = PlayerAnimType::Idle;
                    if (!state.isOnGround_) {
                        targetType = PlayerAnimType::Jump; // 空中・落下
                    } else if (std::abs(state.velocity_.x) > 0.1f) {
                        targetType = PlayerAnimType::Walk;
                    } else {
                        targetType = PlayerAnimType::Idle;
                    }

                    // アニメーションが切り替わった場合（移動開始・停止、ジャンプなど）
                    if (currentAnimType_ != targetType) {
                        bool isSameAnim = false;
                        if ((currentAnimType_ == PlayerAnimType::Idle && targetType == PlayerAnimType::Walk) ||
                            (currentAnimType_ == PlayerAnimType::Walk && targetType == PlayerAnimType::Idle)) {
                            // walkAnimation_ が未作成で idleAnimation_ が使われている場合は時間を巻き戻さない
                            if (walkAnimation_.duration == idleAnimation_.duration && walkAnimation_.nodeAnimations.size() == idleAnimation_.nodeAnimations.size()) {
                                isSameAnim = true;
                            }
                        }

                        currentAnimType_ = targetType;

                        const Animation* nextAnim = nullptr;
                        switch (targetType) {
                        case PlayerAnimType::Swing:
                            nextAnim = &swingAnimation_;
                            break;
                        case PlayerAnimType::Jump:
                            nextAnim = &jumpAnimation_;
                            break;
                        case PlayerAnimType::Walk:
                            nextAnim = &walkAnimation_;
                            break;
                        case PlayerAnimType::Idle:
                            nextAnim = &idleAnimation_;
                            break;
                        default:
                            break;
                        }

                        if (nextAnim) {
                            if (isSameAnim) {
                                animator_->SetAnimation(*nextAnim);
                            } else {
                                // 0.15秒で滑らかにクロスフェード（歩行停止時などの不自然な姿勢ジャンプを防止）
                                animator_->CrossFade(*nextAnim, 0.15f);
                            }
                        }
                    }
                }
            }
            animator_->Update();
        }

        // gaikotuモデルは足元原点のため、当たり判定の底辺に合わせるようY軸をオフセットする
        Vector3 modelPos = state.position_;
        modelPos.y -= params.halfHeight_;
        
        float rotationY = modelObj_->GetRotation().y;
        if (state.isWallClinging_ || state.isWallSliding_) {
            if (state.isTouchingWallRight_) {
                rotationY = -1.57079632f;
                modelPos.x -= 0.2f; // 右壁から少し離す（左へずらす）
            } else if (state.isTouchingWallLeft_) {
                rotationY = 1.57079632f;
                modelPos.x += 0.2f; // 左壁から少し離す（右へずらす）
            }
        } else if (state.isSwingingChain_ || state.isHoldingChain_) {
            rotationY = 0.0f; // スイング中・宝石持ち中は正面（カメラ目線）
        } else if (state.faceDirX_ < -0.01f || state.faceDirX_ > 0.01f) {
            // 向きの指定があればそちらを優先（投げた直後など、動かないまま左右を向く）
            rotationY = (state.faceDirX_ < 0.0f) ? 1.57079632f : -1.57079632f;
        } else {
            if (state.velocity_.x < -0.01f) {
                rotationY = 1.57079632f;
            } else if (state.velocity_.x > 0.01f) {
                rotationY = -1.57079632f;
            }
        }
        
        modelObj_->SetTranslation(modelPos);
        
        float baseScale = (params.modelScale_ > 0.0f) ? params.modelScale_ : 2.0f;

        float flipAngleZ = 0.0f;
        if (state.spinFlipTimer_ > 0.0f && state.spinFlipDuration_ > 0.0f) {
            float p = 1.0f - (state.spinFlipTimer_ / state.spinFlipDuration_);
            p = std::clamp(p, 0.0f, 1.0f);
            flipAngleZ = state.spinFlipSign_ * (2.0f * 3.14159265f) * p;
        }

        if (state.isDashing_) {
            modelObj_->GetMaterial().color = params.colorDashed_;
            Vector3 dashDir = state.velocity_;
            dashDir.z = 0.0f;
            float speed = 1.0f;
            if (dashDir.x != 0.0f || dashDir.y != 0.0f) {
                float length = std::sqrt(dashDir.x * dashDir.x + dashDir.y * dashDir.y);
                if (length > 0.0f) { dashDir.x /= length; dashDir.y /= length; speed = length; }
            }
            float stretch = 1.0f + (speed * 0.02f);
            float squash = 1.0f / stretch;
            modelObj_->SetScale({ baseScale * stretch, baseScale * squash, baseScale });
            modelObj_->SetRotation({ 0.0f, rotationY, 0.0f });
        } else {
            if (state.stamina_ <= params.maxStamina_ * 0.2f || state.isExhausted_) {
                float blink = std::sin(visualTime_ * 40.0f);
                if (blink > 0.0f) {
                    modelObj_->GetMaterial().color = params.colorTired_;
                } else {
                    modelObj_->GetMaterial().color = { 1.0f, 1.0f, 1.0f, 1.0f };
                }
            } else if (state.stamina_ <= params.maxStamina_ * 0.5f) {
                float blink = std::sin(visualTime_ * 20.0f);
                if (blink > 0.0f) {
                    modelObj_->GetMaterial().color = params.colorTired_;
                } else {
                    modelObj_->GetMaterial().color = params.colorNormal_;
                }
            } else {
                modelObj_->GetMaterial().color = params.colorNormal_;
            }
            modelObj_->SetScale({ baseScale, baseScale, baseScale });
            modelObj_->SetRotation({ 0.0f, rotationY, flipAngleZ });
        }
        
        if (!state.isDead_) {
            modelObj_->Update();

            if (animator_) {
                capePhysics_.Update(animator_.get(), modelObj_->GetWorldMatrix(), state.velocity_, deltaTime);
                animator_->UpdateSkeletonAndSkinCluster();
            }
        }
    }

    // パーティクル更新
    for (auto& confetti : confettiParticles_) {
        if (confetti.active) {
            confetti.timer += deltaTime;
            if (confetti.timer >= confetti.duration) {
                confetti.active = false;
            } else {
                confetti.velocity.y += (params.gravity_ * 0.3f) * deltaTime;
                confetti.velocity.x *= 0.98f; 
                confetti.position.x += confetti.velocity.x * deltaTime;
                confetti.position.y += confetti.velocity.y * deltaTime;
                confetti.rotation.x += confetti.rotationSpeed.x * deltaTime;
                confetti.rotation.y += confetti.rotationSpeed.y * deltaTime;
                confetti.rotation.z += confetti.rotationSpeed.z * deltaTime;
            }
        }
    }
    
    for (auto& dust : dustParticles_) {
        if (dust.active) {
            dust.timer += deltaTime;
            if (dust.timer >= dust.duration) {
                dust.active = false;
            } else {
                dust.position.x += dust.velocity.x * deltaTime;
                dust.position.y += dust.velocity.y * deltaTime;
            }
        }
    }

    for (auto& ring : dashRingParticles_) {
        if (ring.active) {
            ring.timer += deltaTime;
            if (ring.timer >= ring.duration) {
                ring.active = false;
            }
        }
    }

    for (auto& smoke : smokeParticles_) {
        if (smoke.active) {
            smoke.timer += deltaTime;
            if (smoke.timer >= smoke.duration) {
                smoke.active = false;
            } else {
                // 初速の爆発から急速にブレーキをかけて煙のドームを形成
                smoke.velocity.x *= 0.86f;
                smoke.velocity.y *= 0.86f;
                smoke.velocity.y += 0.35f * deltaTime; // ふんわりとした上昇気流
                smoke.position.x += smoke.velocity.x * deltaTime;
                smoke.position.y += smoke.velocity.y * deltaTime;
                smoke.rotation += smoke.rotSpeed * deltaTime;
            }
        }
    }
}

void PlayerVisuals::Draw(const PlayerState& state, const PlayerParams& params) {
    (void)params;
    // 死亡時、またはクリア演出で煙幕に紛れて脱出した後はプレイヤーモデルを描画しない
    if (!state.isDead_ && !state.isClearEscaped_ && primitiveObj_) {
        if (modelObj_) {
            modelObj_->Draw();
        } else {
            primitiveObj_->Draw();
        }
    }
    
    // シャドウパス実行中はパーティクル（砂埃、ダッシュリング、紙吹雪、煙幕）を描画しない
    if (Renderer::GetInstance() && Renderer::GetInstance()->IsShadowPass()) {
        return;
    }

    if (dashRingPrimitive_) {
        dashRingPrimitive_->ResetGhostIndex();
        for (const auto& ring : dashRingParticles_) {
            if (ring.active) {
                float t = ring.timer / ring.duration;
                // 最初速く広がり、だんだんゆっくりになる自然なイーズアウトに変更
                float easedT = 1.0f - (1.0f - t) * (1.0f - t); 
                float currentSize = ring.startSize + (ring.endSize - ring.startSize) * easedT;
                float alpha = 1.0f - (ring.timer / ring.duration);
                alpha = alpha * alpha; 
                
                EulerTransform tr;
                tr.translate = ring.position;
                tr.scale = { currentSize, currentSize, currentSize };
                tr.rotate = ring.rotation;
                
                Material m = dashRingPrimitive_->GetMaterial();
                m.color = { 1.0f, 1.0f, 1.0f, alpha };
                
                dashRingPrimitive_->DrawGhost(tr, m);
            }
        }
    }

    if (dustPrimitive_) {
        dustPrimitive_->ResetGhostIndex();
        for (const auto& dust : dustParticles_) {
            if (dust.active) {
                float alpha = 1.0f - (dust.timer / dust.duration);
                float currentSize = dust.startSize * alpha;
                EulerTransform t;
                t.translate = dust.position;
                t.scale = { currentSize, currentSize, currentSize };
                t.rotate = { 0.0f, 0.0f, 0.0f };
                
                Material m = dustPrimitive_->GetMaterial();
                m.color.w = alpha;
                
                dustPrimitive_->DrawGhost(t, m);
            }
        }
    }

    if (confettiPrimitive_) {
        confettiPrimitive_->ResetGhostIndex();
        for (const auto& confetti : confettiParticles_) {
            if (confetti.active) {
                float currentSize = confetti.size;
                EulerTransform t;
                t.translate = confetti.position;
                t.scale = { currentSize, currentSize, currentSize };
                t.rotate = confetti.rotation;
                
                Material m = confettiPrimitive_->GetMaterial();
                m.color = confetti.color;
                
                confettiPrimitive_->DrawGhost(t, m);
            }
        }
    }

    // 煙幕（スモークボム）の描画
    if (smokePrimitive_) {
        smokePrimitive_->ResetGhostIndex();
        for (const auto& smoke : smokeParticles_) {
            if (smoke.active) {
                float progress = smoke.timer / smoke.duration;
                // 最初の0.25秒で一気に爆発的膨張（ドカンと広がる）
                float expandT = std::clamp(progress / 0.25f, 0.0f, 1.0f);
                float easedExpand = 1.0f - (1.0f - expandT) * (1.0f - expandT) * (1.0f - expandT);
                float currentSize = smoke.startSize + (smoke.endSize - smoke.startSize) * easedExpand;

                // 最初0.08秒で急激に白煙が立ち上がり、0.45秒まで怪盗を完全に隠す濃さを維持
                // その後ふんわりと滑らかに消えていく
                float alpha = 0.0f;
                if (progress < 0.08f) {
                    alpha = (progress / 0.08f) * 0.95f;
                } else if (progress < 0.45f) {
                    alpha = 0.95f;
                } else {
                    float fadeT = (progress - 0.45f) / 0.55f;
                    alpha = 0.95f * (1.0f - fadeT * fadeT);
                }

                EulerTransform t;
                t.translate = smoke.position;
                t.scale = { currentSize, currentSize, 0.02f }; // 正方形クアッド板として展開
                t.rotate = { 0.0f, 0.0f, smoke.rotation };

                Material m = smokePrimitive_->GetMaterial();
                m.color = { smoke.color.x, smoke.color.y, smoke.color.z, alpha };

                smokePrimitive_->DrawGhost(t, m);
            }
        }
    }
}

void PlayerVisuals::SpawnJumpDust(const Vector3& basePos, float dirX) {
    static std::mt19937 randEngine(std::random_device{}());
    std::uniform_real_distribution<float> velDistX(-4.0f, 4.0f);
    std::uniform_real_distribution<float> velDistY(2.0f, 6.0f);
    // サイズを半分に
    std::uniform_real_distribution<float> sizeDist(0.15f, 0.3f);
    std::uniform_real_distribution<float> durationDist(0.2f, 0.4f);

    for (int i = 0; i < 8; ++i) {
        DustParticle dust;
        dust.position = basePos;
        // 壁キック時はdirX方向に少し勢いをつける
        dust.velocity = { velDistX(randEngine) + dirX * 6.0f, velDistY(randEngine), 0.0f };
        dust.timer = 0.0f;
        dust.duration = durationDist(randEngine);
        dust.startSize = sizeDist(randEngine);
        dust.active = true;
        
        bool reused = false;
        for (auto& existing : dustParticles_) {
            if (!existing.active) {
                existing = dust;
                reused = true;
                break;
            }
        }
        if (!reused) {
            dustParticles_.push_back(dust);
        }
    }
}
void PlayerVisuals::SpawnRunDust(const Vector3& basePos, float dirX) {
    static std::mt19937 randEngine(std::random_device{}());
    std::uniform_real_distribution<float> velDistX(-2.0f, 2.0f);
    std::uniform_real_distribution<float> velDistY(1.0f, 3.0f);
    // サイズを半分に
    std::uniform_real_distribution<float> sizeDist(0.125f, 0.25f);
    std::uniform_real_distribution<float> durationDist(0.15f, 0.35f);

    for (int i = 0; i < 4; ++i) {
        DustParticle dust;
        dust.position = basePos;
        // 走っている方向と逆に飛ぶようにする
        dust.velocity = { velDistX(randEngine) - dirX * 4.0f, velDistY(randEngine), 0.0f };
        dust.timer = 0.0f;
        dust.duration = durationDist(randEngine);
        dust.startSize = sizeDist(randEngine);
        dust.active = true;
        
        bool reused = false;
        for (auto& existing : dustParticles_) {
            if (!existing.active) {
                existing = dust;
                reused = true;
                break;
            }
        }
        if (!reused) {
            dustParticles_.push_back(dust);
        }
    }
}
void PlayerVisuals::SpawnConfetti(const Vector3& pos) {
    static std::mt19937 randEngine(std::random_device{}());
    // もっと派手にするため勢いを上げる
    std::uniform_real_distribution<float> velDistX(-8.0f, 8.0f);
    std::uniform_real_distribution<float> velDistY(8.0f, 15.0f);
    // サイズを半分に
    std::uniform_real_distribution<float> sizeDist(0.125f, 0.3f);
    std::uniform_real_distribution<float> durationDist(2.0f, 5.0f);
    std::uniform_real_distribution<float> rotDist(0.0f, 6.28f);
    std::uniform_real_distribution<float> rotSpeedDist(-10.0f, 10.0f);
    
    std::vector<Vector4> colors = {
        {1.0f, 0.2f, 0.2f, 1.0f}, // Red
        {0.2f, 1.0f, 0.2f, 1.0f}, // Green
        {0.2f, 0.2f, 1.0f, 1.0f}, // Blue
        {1.0f, 0.8f, 0.0f, 1.0f}, // Yellow
        {1.0f, 0.2f, 1.0f, 1.0f}, // Magenta
        {0.0f, 0.8f, 1.0f, 1.0f}  // Cyan
    };
    std::uniform_int_distribution<int> colorDist(0, static_cast<int>(colors.size()) - 1);

    // 枚数を増やして派手にする
    for (int i = 0; i < 100; ++i) {
        ConfettiParticle confetti;
        confetti.position = pos;
        confetti.velocity = { velDistX(randEngine), velDistY(randEngine), 0.0f };
        confetti.color = colors[colorDist(randEngine)];
        confetti.rotation = { rotDist(randEngine), rotDist(randEngine), rotDist(randEngine) };
        confetti.rotationSpeed = { rotSpeedDist(randEngine), rotSpeedDist(randEngine), rotSpeedDist(randEngine) };
        confetti.timer = 0.0f;
        confetti.duration = durationDist(randEngine);
        confetti.size = sizeDist(randEngine);
        confetti.active = true;
        
        bool reused = false;
        for (auto& existing : confettiParticles_) {
            if (!existing.active) {
                existing = confetti;
                reused = true;
                break;
            }
        }
        if (!reused) {
            confettiParticles_.push_back(confetti);
        }
    }
}
float PlayerVisuals::EaseInElastic(float t) const {
    const float c4 = (2.0f * 3.14159265f) / 3.0f;
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;
    return -std::pow(2.0f, 10.0f * t - 10.0f) * std::sin((t * 10.0f - 10.75f) * c4);
}
void PlayerVisuals::SpawnDashRing(const Vector3& basePos, const Vector3& dashDir) {
    // ダッシュの方向に基づいて角度を計算
    float angle = std::atan2(dashDir.y, dashDir.x);
    
    // リングを半分の大きさに調整
    float currentStartSize = 0.5f;
    float currentEndSize = 3.0f;

    for (int i = 0; i < 3; ++i) {
        DashRingParticle ring;
        // 背景などに埋もれないようにZ座標をわずかに手前(-0.1f)にする
        // 複数出す場合はZファイティングを防ぐため少しずつZをずらす
        ring.position = { basePos.x, basePos.y, basePos.z - 0.1f - (i * 0.01f) };
        
        // PrimitiveRingはXY平面上に生成されるため、
        // X軸周りに少し傾けて(1.0f)楕円形(3Dっぽく)にし、
        // Z軸周りに回転させて楕円の短軸が進行方向を向くようにする。
        ring.rotation = { 1.0f, 0.0f, angle - 1.5708f }; 

        ring.timer = 0.0f;
        ring.duration = 0.4f; // 少しだけ長持ちさせる
        ring.startSize = currentStartSize;
        ring.endSize = currentEndSize; // 大きく広がる
        ring.active = true;

        bool reused = false;
        for (auto& existing : dashRingParticles_) {
            if (!existing.active) {
                existing = ring;
                reused = true;
                break;
            }
        }
        if (!reused) {
            dashRingParticles_.push_back(ring);
        }

        // 3つのリングがそれぞれはっきり異なる大きさ（半分ずつ）になるように戻す
        currentStartSize *= 0.5f;
        currentEndSize *= 0.5f;
    }
}

void PlayerVisuals::SpawnSmokeBomb(const Vector3& pos) {
    Log(std::format("PlayerVisuals: SpawnSmokeBomb called at ({:.2f}, {:.2f}, {:.2f})\n", pos.x, pos.y, pos.z));
    static std::mt19937 randEngine(std::random_device{}());
    std::uniform_real_distribution<float> angleDist(0.0f, 6.2831853f);
    std::uniform_real_distribution<float> rotSpeedDist(-3.0f, 3.0f);
    std::uniform_real_distribution<float> rotInitDist(0.0f, 6.2831853f);
    std::uniform_real_distribution<float> durationDist(1.6f, 2.3f);
    std::uniform_real_distribution<float> zDist(-0.35f, -0.15f);

    // 1. コア爆発煙（中心から一気に膨らみ、プレイヤーを完全に隠す巨大な白煙の塊）: 20個
    std::uniform_real_distribution<float> coreSpeed(1.2f, 3.5f);
    std::uniform_real_distribution<float> coreStartSize(0.8f, 1.2f);
    std::uniform_real_distribution<float> coreEndSize(3.8f, 5.2f);
    for (int i = 0; i < 20; ++i) {
        float angle = angleDist(randEngine);
        float spd = coreSpeed(randEngine);
        SmokeParticle p;
        p.position = { pos.x + std::cos(angle) * 0.15f, pos.y + 0.35f + std::sin(angle) * 0.25f, zDist(randEngine) };
        p.velocity = { std::cos(angle) * spd, std::sin(angle) * spd * 0.7f + 0.8f, 0.0f };
        p.color = { 1.0f, 1.0f, 1.0f, 1.0f }; // 純白
        p.startSize = coreStartSize(randEngine);
        p.endSize = coreEndSize(randEngine);
        p.timer = 0.0f;
        p.duration = durationDist(randEngine);
        p.rotation = rotInitDist(randEngine);
        p.rotSpeed = rotSpeedDist(randEngine);
        p.active = true;

        bool reused = false;
        for (auto& existing : smokeParticles_) {
            if (!existing.active) { existing = p; reused = true; break; }
        }
        if (!reused) smokeParticles_.push_back(p);
    }

    // 2. 放射状バースト煙（全方位に勢いよく飛び散り、煙幕の輪郭をダイナミックに広げる）: 44個
    std::uniform_real_distribution<float> burstSpeed(4.0f, 8.0f);
    std::uniform_real_distribution<float> burstStartSize(0.6f, 1.0f);
    std::uniform_real_distribution<float> burstEndSize(2.8f, 4.0f);
    for (int i = 0; i < 44; ++i) {
        float angle = angleDist(randEngine);
        float spd = burstSpeed(randEngine);
        SmokeParticle p;
        p.position = { pos.x + std::cos(angle) * 0.2f, pos.y + 0.3f + std::sin(angle) * 0.2f, zDist(randEngine) };
        p.velocity = { std::cos(angle) * spd, std::sin(angle) * spd * 0.8f + 1.0f, 0.0f };
        p.color = { 0.98f, 0.98f, 1.0f, 1.0f }; // 純白
        p.startSize = burstStartSize(randEngine);
        p.endSize = burstEndSize(randEngine);
        p.timer = 0.0f;
        p.duration = durationDist(randEngine);
        p.rotation = rotInitDist(randEngine);
        p.rotSpeed = rotSpeedDist(randEngine);
        p.active = true;

        bool reused = false;
        for (auto& existing : smokeParticles_) {
            if (!existing.active) { existing = p; reused = true; break; }
        }
        if (!reused) smokeParticles_.push_back(p);
    }
}

void PlayerVisuals::ClearEffects() {
    dustParticles_.clear();
    confettiParticles_.clear();
    dashRingParticles_.clear();
    smokeParticles_.clear();
    currentAnimType_ = PlayerAnimType::None;
}

#ifdef USE_IMGUI
#include "../../externals/imgui/imgui.h"

void PlayerVisuals::DisplayImGui() {
    if (ImGui::TreeNode("しがみつき姿勢 (Cling Pose Edit)")) {
        ImGui::SliderFloat3("体（腰）回転 (Hips X,Y,Z)", debugHipsRot_, -3.14f, 3.14f);
        ImGui::SliderFloat3("左肩回転 (LArm X,Y,Z)", debugLArmRot_, -3.14f, 3.14f);
        ImGui::SliderFloat3("右肩回転 (RArm X,Y,Z)", debugRArmRot_, -3.14f, 3.14f);
        ImGui::SliderFloat3("左肘回転 (LForeArm X,Y,Z)", debugLForeArmRot_, -3.14f, 3.14f);
        ImGui::SliderFloat3("右肘回転 (RForeArm X,Y,Z)", debugRForeArmRot_, -3.14f, 3.14f);
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("マント揺れもの物理 (Cape Physics)")) {
        CapeParams& cp = capePhysics_.GetParams();
        ImGui::SliderFloat("復元バネ力 (Stiffness)", &cp.stiffness, 0.05f, 0.8f);
        ImGui::SliderFloat("空気抵抗 (Damping)", &cp.damping, 0.5f, 0.98f);
        ImGui::SliderFloat("慣性追従率 (Inertia)", &cp.inertiaFactor, 0.0f, 2.0f);
        ImGui::SliderFloat("そよ風の強さ (Wind)", &cp.windStrength, 0.0f, 0.5f);
        ImGui::SliderFloat3("重力 (Gravity)", &cp.gravity.x, -20.0f, 10.0f);
        ImGui::TreePop();
    }
}
#endif