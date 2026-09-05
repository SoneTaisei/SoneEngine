#include "Graphics/TextureManager.h"
#include "PlayerVisuals.h"
#include "Core/TimeManager.h"
#include "Renderer/Renderer.h"
#include <random>
#include <cmath>

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

        idleAnimation_ = LoadAnimationFile("resources/Object/Original/gaikotu", "scene.gltf", "Idle");
        walkAnimation_ = LoadAnimationFile("resources/Object/Original/gaikotu", "scene.gltf", "Walk");
        jumpAnimation_ = LoadAnimationFile("resources/Object/Original/gaikotu", "scene.gltf", "Jump");

        if (!LoadAnimationFromJsonFile(wallClimbAnimation_, "resources/json/shared/Player/wall_climb_animation.json")) {
            wallClimbAnimation_ = CreateDefaultWallClimbAnimation();
        }
        if (!LoadAnimationFromJsonFile(holdingWallAnimation_, "resources/json/shared/Player/holding_wall.json")) {
            holdingWallAnimation_ = idleAnimation_;
        }
        if (!LoadAnimationFromJsonFile(airDashAnimation_, "resources/json/shared/Player/air_dash_animation.json")) {
            airDashAnimation_ = CreateDefaultAirDashAnimation();
        }

        animator_->SetAnimation(idleAnimation_);
        animator_->Play();

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
}

void PlayerVisuals::Update(const PlayerState& state, const PlayerParams& params, float deltaTime) {
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
            } else {
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

                    // アニメーションが切り替わった場合（落下開始時など）に毎回時間を0.0fにリセット
                    if (currentAnimType_ != targetType) {
                        currentAnimType_ = targetType;
                        animator_->SetTime(0.0f);

                        switch (targetType) {
                        case PlayerAnimType::Jump:
                            animator_->SetAnimation(jumpAnimation_);
                            break;
                        case PlayerAnimType::Walk:
                            animator_->SetAnimation(walkAnimation_);
                            break;
                        case PlayerAnimType::Idle:
                            animator_->SetAnimation(idleAnimation_);
                            break;
                        default:
                            break;
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
        } else {
            if (state.velocity_.x < -0.01f) {
                rotationY = 1.57079632f;
            } else if (state.velocity_.x > 0.01f) {
                rotationY = -1.57079632f;
            }
        }
        
        modelObj_->SetTranslation(modelPos);
        
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
            modelObj_->SetScale({ 1.0f * stretch, 1.0f * squash, 1.0f });
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
            modelObj_->SetScale({ 1.0f, 1.0f, 1.0f });
            modelObj_->SetRotation({ 0.0f, rotationY, 0.0f });
        }
        
        if (!state.isDead_) {
            modelObj_->Update();
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
}

void PlayerVisuals::Draw(const PlayerState& state, const PlayerParams& params) {
    if (state.isDead_ && primitiveObj_) {
        // メインオブジェクトを描画せず、6x6のブロックに対してゴースト描画する
        primitiveObj_->ResetGhostIndex();
        float dissolveProgress = state.deathTimer_ / params.deathDuration_;
        
        for (int y = 0; y < 6; ++y) {
            for (int x = 0; x < 6; ++x) {
                float bx = (float)x;
                float by = (float)(5 - y);
                // シェーダーと同じ乱数計算をCPUで行う
                float dot_val = bx * 12.9898f + by * 78.233f;
                float s = std::sin(dot_val);
                float n = s * 43758.5453f;
                n = n - std::floor(n);
                
                float cellW = (params.halfWidth_ * 2.0f) / 6.0f;
                float cellH = (params.halfHeight_ * 2.0f) / 6.0f;
                float offsetX = -params.halfWidth_ + cellW * 0.5f + cellW * x;
                float offsetY = -params.halfHeight_ + cellH * 0.5f + cellH * y;
                
                EulerTransform ghostTransform;
                ghostTransform.scale = { cellW, cellH, 1.0f };
                ghostTransform.rotate = {0.0f, 0.0f, 0.0f};
                
                Vector3 basePos = state.position_;
                basePos.x += offsetX;
                basePos.y += offsetY;
                
                Material ghostMat = primitiveObj_->GetMaterial();
                ghostMat.dissolveThreshold = 0.0f; // ゴースト自体はディゾルブさせない
                
                if (dissolveProgress > n) {
                    // ディゾルブ時間を超えたブロックは上に飛んでいく
                    float timeSinceDissolve = (dissolveProgress - n) * params.deathDuration_;
                    basePos.y += timeSinceDissolve * 10.0f; 
                    ghostTransform.rotate.z = timeSinceDissolve * 15.0f; // 回転を加える
                    
                    // 徐々に小さくする
                    float shrink = 1.0f - (timeSinceDissolve / 0.1f);
                    if (shrink < 0.0f) shrink = 0.0f;
                    ghostTransform.scale.x *= shrink;
                    ghostTransform.scale.y *= shrink;
                    ghostMat.color.w *= shrink; // 透明度も下げる
                }
                
                if (ghostTransform.scale.x > 0.0f) {
                    ghostTransform.translate = basePos;
                    // DrawGhostは内部的に引数無しのGetCommandListを使っている想定
                    primitiveObj_->DrawGhost(ghostTransform, ghostMat);
                }
            }
        }
    } else if (!state.isDead_ && primitiveObj_) {
        if (modelObj_) {
            modelObj_->Draw();
        } else {
            primitiveObj_->Draw();
        }
    }
    
    // シャドウパス実行中はパーティクル（砂埃、ダッシュリング、紙吹雪）を描画しない（不要かつシャドウパス破壊防止）
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

void PlayerVisuals::ClearEffects() {
    dustParticles_.clear();
    confettiParticles_.clear();
    dashRingParticles_.clear();
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
}
#endif