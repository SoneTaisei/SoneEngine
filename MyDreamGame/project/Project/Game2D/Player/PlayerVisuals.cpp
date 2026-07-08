#include "Graphics/TextureManager.h"
#include "PlayerVisuals.h"
#include <random>
#include <cmath>

void PlayerVisuals::Initialize(ID3D12Device* device, Primitive* boxPrimitive, Primitive* ringPrimitive, uint32_t texHandle) {
    primitiveObj_ = std::make_unique<PrimitiveObject>();
    primitiveObj_->Initialize(device, boxPrimitive);
    primitiveObj_->SetName("Player");
    primitiveObj_->SetTextureHandle(TextureManager::GetInstance()->GetGpuHandle(texHandle));
    primitiveObj_->GetMaterial().lightingType = 1;

    dashRingPrimitive_ = std::make_unique<PrimitiveObject>();
    dashRingPrimitive_->Initialize(device, ringPrimitive);
    dashRingPrimitive_->SetName("DashRing");
    dashRingPrimitive_->SetTextureHandle(TextureManager::GetInstance()->GetGpuHandle(texHandle));
    dashRingPrimitive_->GetMaterial().lightingType = 0;
    dashRingPrimitive_->GetMaterial().enableEnvironmentMap = 0;
    dashRingPrimitive_->SetIsBillboard(false);
    dashRingPrimitive_->SetIsDoubleSided(true);
    dashRingPrimitive_->SetBlendMode(BlendMode::kBlendModeAdd);
}

void PlayerVisuals::Update(const PlayerState& state, const PlayerParams& params, float deltaTime) {
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
            primitiveObj_->GetMaterial().color = params.colorNormal_;
            primitiveObj_->SetScale({ params.halfWidth_ * 2.0f, params.halfHeight_ * 2.0f, 1.0f });
            primitiveObj_->SetRotation({ 0.0f, 0.0f, 0.0f });
        }
        
        if (!state.isDead_) {
            primitiveObj_->Update();
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
                
                Transform ghostTransform;
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
        primitiveObj_->Draw();
    }
    
    if (dashRingPrimitive_) {
        for (const auto& ring : dashRingParticles_) {
            if (ring.active) {
                float t = ring.timer / ring.duration;
                float easedT = EaseInElastic(t);
                float currentSize = ring.startSize + (ring.endSize - ring.startSize) * easedT;
                float alpha = 1.0f - (ring.timer / ring.duration);
                alpha = alpha * alpha; 
                dashRingPrimitive_->SetTranslation(ring.position);
                dashRingPrimitive_->SetScale({ currentSize, currentSize, currentSize });
                dashRingPrimitive_->SetRotation(ring.rotation);
                dashRingPrimitive_->GetMaterial().color = { 1.0f, 1.0f, 1.0f, alpha };
                dashRingPrimitive_->Update();
                dashRingPrimitive_->Draw();
            }
        }
    }
}

void PlayerVisuals::SpawnJumpDust(const Vector3& basePos, float dirX) {
    static std::mt19937 randEngine(std::random_device{}());
    std::uniform_real_distribution<float> velDistX(-3.0f, 3.0f);
    std::uniform_real_distribution<float> velDistY(1.0f, 4.0f);
    std::uniform_real_distribution<float> sizeDist(0.1f, 0.3f);
    std::uniform_real_distribution<float> durationDist(0.15f, 0.35f);

    for (int i = 0; i < 5; ++i) {
        DustParticle dust;
        dust.position = basePos;
        // 壁キック時はdirX方向に少し勢いをつける
        dust.velocity = { velDistX(randEngine) + dirX * 5.0f, velDistY(randEngine), 0.0f };
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
    std::uniform_real_distribution<float> velDistX(-1.0f, 1.0f);
    std::uniform_real_distribution<float> velDistY(0.5f, 2.0f);
    std::uniform_real_distribution<float> sizeDist(0.1f, 0.2f);
    std::uniform_real_distribution<float> durationDist(0.1f, 0.25f);

    for (int i = 0; i < 2; ++i) {
        DustParticle dust;
        dust.position = basePos;
        // 走っている方向と逆に飛ぶようにする
        dust.velocity = { velDistX(randEngine) - dirX * 3.0f, velDistY(randEngine), 0.0f };
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
    // 飛ぶ勢いを半分程度に落とす
    std::uniform_real_distribution<float> velDistX(-4.0f, 4.0f);
    std::uniform_real_distribution<float> velDistY(4.0f, 9.0f);
    std::uniform_real_distribution<float> sizeDist(0.1f, 0.25f);
    std::uniform_real_distribution<float> durationDist(2.0f, 4.0f);
    std::uniform_real_distribution<float> rotDist(0.0f, 6.28f);
    std::uniform_real_distribution<float> rotSpeedDist(-5.0f, 5.0f);
    
    std::vector<Vector4> colors = {
        {1.0f, 0.2f, 0.2f, 1.0f}, // Red
        {0.2f, 1.0f, 0.2f, 1.0f}, // Green
        {0.2f, 0.2f, 1.0f, 1.0f}, // Blue
        {1.0f, 0.8f, 0.0f, 1.0f}, // Yellow
        {1.0f, 0.2f, 1.0f, 1.0f}, // Magenta
        {0.0f, 0.8f, 1.0f, 1.0f}  // Cyan
    };
    std::uniform_int_distribution<int> colorDist(0, static_cast<int>(colors.size()) - 1);

    // 枚数も少し減らして派手すぎないように調整
    for (int i = 0; i < 40; ++i) {
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
    
    float currentStartSize = 0.25f;
    float currentEndSize = 1.5f;

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
        ring.duration = 0.3f; // 短い時間で消える
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

        // 次のリングのサイズを半分にする
        currentStartSize *= 0.5f;
        currentEndSize *= 0.5f;
    }
}