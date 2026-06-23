#include "Player2D.h"
#include "MapChip2D.h"
#include "Core/Utility/TransformFunctions.h"
#include "Graphics/TextureManager.h"
#ifdef USE_IMGUI
#include "../externals/imgui/imgui.h"
#endif

void Player2D::Initialize(ID3D12GraphicsCommandList* commandList) {
    Microsoft::WRL::ComPtr<ID3D12Device> device;
    commandList->GetDevice(IID_PPV_ARGS(&device));

    // Box型プリミティブを取得して PrimitiveObject を初期化
    Primitive* boxPrimitive = PrimitiveManager::GetInstance()->GetPrimitive(PrimitiveType::Box, 1.0f);
    primitiveObj_ = std::make_unique<PrimitiveObject>();
    primitiveObj_->Initialize(device.Get(), boxPrimitive);
    primitiveObj_->SetName("Player");

    // デフォルトのテクスチャ（白）をロードして設定
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> comPtrCommandList(commandList);
    uint32_t texHandle = TextureManager::GetInstance()->Load("resources/Object/School/human/white.png", comPtrCommandList);
    primitiveObj_->SetTextureHandle(TextureManager::GetInstance()->GetGpuHandle(texHandle));

    // プレイヤーの見た目設定
    primitiveObj_->SetScale({ halfWidth_ * 2.0f, halfHeight_ * 2.0f, 1.0f });
    primitiveObj_->GetMaterial().color = colorNormal_; // 青色
    primitiveObj_->GetMaterial().color = colorNormal_; // 青色
    primitiveObj_->GetMaterial().lightingType = 0; // ライティング無効（2Dなので）

    // ダッシュ波紋エフェクト用リングの初期化
    Primitive* ringPrimitive = PrimitiveManager::GetInstance()->GetRing(0.8f, 1.0f, 32, 0.0f, 2.0f * 3.14159f, {1,1,1,1}, {1,1,1,1}, false);
    dashRingPrimitive_ = std::make_unique<PrimitiveObject>();
    dashRingPrimitive_->Initialize(device.Get(), ringPrimitive);
    dashRingPrimitive_->SetName("DashRing");
    dashRingPrimitive_->SetTextureHandle(TextureManager::GetInstance()->GetGpuHandle(texHandle));
    dashRingPrimitive_->GetMaterial().lightingType = 0;
    dashRingPrimitive_->GetMaterial().enableEnvironmentMap = 0;
    dashRingPrimitive_->SetIsBillboard(false);
    dashRingPrimitive_->SetIsDoubleSided(true);
    dashRingPrimitive_->SetBlendMode(BlendMode::kBlendModeAdd);
}

void Player2D::FindSpawnPoint(const MapChip2D& map) {
    for (int y = 0; y < map.GetHeight(); ++y) {
        for (int x = 0; x < map.GetWidth(); ++x) {
            if (map.GetChipType(x, y) == MapChip2D::ChipType::kPlayerSpawn) {
                // スポーン地点の中心座標を計算
                startPosition_.x = map.ChipToWorldX(x) + map.GetChipSize() * 0.5f;
                startPosition_.y = map.ChipToWorldY(y) + map.GetChipSize() * 0.5f;
                position_ = startPosition_;
                return;
            }
        }
    }
}

void Player2D::Update(MapChip2D& map, bool isTransitioning) {
    float deltaTime = TimeManager::GetInstance().GetDeltaTime();

    if (isGoal_) {
        // ゴール演出時のプレイヤーは静止させる
        goalTimer_ += deltaTime;
        velocity_.x = 0.0f;
        velocity_.y = 0.0f;
        
        // 紙吹雪パーティクルの更新はスキップしない
        for (auto& confetti : confettiParticles_) {
            if (confetti.active) {
                confetti.timer += deltaTime;
                if (confetti.timer >= confetti.duration) {
                    confetti.active = false;
                } else {
                    // 重力を少し弱めにかける (ひらひら落ちる感じ)
                    confetti.velocity.y += (gravity_ * 0.3f) * deltaTime;
                    // 空気抵抗
                    confetti.velocity.x *= 0.98f; 
                    
                    confetti.position.x += confetti.velocity.x * deltaTime;
                    confetti.position.y += confetti.velocity.y * deltaTime;
                    
                    confetti.rotation.x += confetti.rotationSpeed.x * deltaTime;
                    confetti.rotation.y += confetti.rotationSpeed.y * deltaTime;
                    confetti.rotation.z += confetti.rotationSpeed.z * deltaTime;
                }
            }
        }
        return;
    }

    // 死亡演出中の更新処理
    if (isDead_) {
        // スローモーション中は実時間が遅くなるため、deathTimer_にはdeltaTimeを足していく。
        deathTimer_ += deltaTime;

        // ノックバック物理挙動（演出中ずっと続ける）
        velocity_.y += gravity_ * deltaTime;
        position_.x += velocity_.x * deltaTime;
        position_.y += velocity_.y * deltaTime;

        if (deathTimer_ >= deathDuration_) {
            // スタート地点に復活
            position_ = startPosition_;
            velocity_ = { 0.0f, 0.0f, 0.0f };
            isDead_ = false;
            deathTimer_ = 0.0f;
            isDashing_ = false;
            canDash_ = true;
            // パラメータをリセット
            primitiveObj_->GetMaterial().dissolveThreshold = 0.0f;
            TimeManager::GetInstance().SetTimeScale(1.0f); // スローモーション解除
            
            // リスポーン演出の開始
            isRespawning_ = true;
            respawnTimer_ = 0.0f;
            primitiveObj_->SetScale({ 0.0f, 0.0f, 1.0f });
        }

        // PrimitiveObjectの座標を更新
        primitiveObj_->SetTranslation(position_);
        primitiveObj_->Update();
        return;
    }

    // リスポーン時のスケール拡大演出（この間は操作・物理無効）
    if (isRespawning_) {
        respawnTimer_ += deltaTime;
        float t = (std::min)(respawnTimer_ / respawnDuration_, 1.0f);
        
        // EaseOutBackによる弾むようなポップアップ
        float c1 = 1.70158f;
        float c3 = c1 + 1.0f;
        float p = t - 1.0f;
        float scaleProgress = 1.0f + c3 * (p * p * p) + c1 * (p * p);
        if (scaleProgress < 0.0f) scaleProgress = 0.0f;

        primitiveObj_->SetScale({ halfWidth_ * 2.0f * scaleProgress, halfHeight_ * 2.0f * scaleProgress, 1.0f });

        if (t >= 1.0f) {
            isRespawning_ = false;
            primitiveObj_->SetScale({ halfWidth_ * 2.0f, halfHeight_ * 2.0f, 1.0f });
        }

        primitiveObj_->GetMaterial().color = colorNormal_; // リスポーン中は通常色
        primitiveObj_->SetTranslation(position_);
        primitiveObj_->Update();
        return; // ここでリターンして通常のゲームロジックをスキップ
    }

    // カメラスライド（ルーム遷移）中の硬直処理
    if (isTransitioning) {
        // 遷移中は操作も物理挙動（重力など）も行わず、時間を止める。
        // ただし、遷移前の速度（velocity_）は保持しておくことで、遷移完了後にジャンプの勢いなどをそのまま引き継ぐ。
        
        // アニメーション等の描画だけは更新する
        primitiveObj_->SetTranslation(position_);
        primitiveObj_->Update();
        return;
    }

    // 壁ジャンプタイマーの更新
    if (wallJumpTimer_ > 0.0f) {
        wallJumpTimer_ -= deltaTime;
    }

    // 入力処理
    HandleInput();

    // ダッシュタイマーの更新
    if (isDashing_) {
        dashTimer_ += deltaTime;
        if (dashTimer_ >= dashDuration_) {
            isDashing_ = false;
            // ダッシュ終了時、上向きの速度が残っている場合は設定した上限値にする。
            // そうしないと、ダッシュの速度がそのままジャンプの初速として働き大きく飛びすぎてしまうため。
            if (velocity_.y > dashEndUpwardVelocity_) {
                velocity_.y = dashEndUpwardVelocity_;
            }
        } else {
            // ダッシュ中は固定速度
            velocity_ = dashVelocity_;
        }
    }

    // --- Y軸（上下）の移動と当たり判定 ---
    ApplyGravity(deltaTime);
    position_.y += velocity_.y * deltaTime;
    ResolveCollisionY(map);

    // --- X軸（左右）の移動と当たり判定 ---
    // 足場に乗っている場合は足場の速度を加算
    if (isOnMovingPlatform_ && isOnGround_) {
        position_.x += platformVelocity_.x * deltaTime;
        position_.y += platformVelocity_.y * deltaTime; // 縦リフト対応
    }
    position_.x += velocity_.x * deltaTime;
    ResolveCollisionX(map);

    // 各種ブロックとの接触判定（デス、ゴール、コイン等）
    SimulateCollisions(map);

    // 画面外落下時のリスポーン演出移行
    if (position_.y < -10.0f) {
        Kill();
    }

    // 色の更新
    primitiveObj_->GetMaterial().color = (isDashing_ || !canDash_) ? colorDashed_ : colorNormal_;

    // 走りエフェクトの発生
    if (isOnGround_ && std::abs(velocity_.x) > 0.1f) {
        runDustTimer_ += deltaTime;
        if (runDustTimer_ >= runDustInterval_) {
            runDustTimer_ = 0.0f;
            float dirX = (velocity_.x > 0.0f) ? 1.0f : -1.0f;
            // プレイヤーの後ろの足元から砂埃を出す
            SpawnRunDust({position_.x - dirX * halfWidth_, position_.y - halfHeight_, 0.0f}, dirX);
        }
    } else {
        runDustTimer_ = 0.0f;
    }

    // 砂埃パーティクルの更新
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

    // ダッシュ波紋パーティクルの更新
    for (auto& ring : dashRingParticles_) {
        if (ring.active) {
            ring.timer += deltaTime;
            if (ring.timer >= ring.duration) {
                ring.active = false;
            }
        }
    }

    // PrimitiveObjectの座標を更新
    primitiveObj_->SetTranslation(position_);
    primitiveObj_->Update();
}

void Player2D::Draw(ID3D12GraphicsCommandList* commandList) {
    if (isDead_) {
        // メインオブジェクトを描画せず、6x6のブロックに分割してゴースト描画する
        primitiveObj_->ResetGhostIndex();
        float dissolveProgress = deathTimer_ / deathDuration_;
        
        for (int y = 0; y < 6; ++y) {
            for (int x = 0; x < 6; ++x) {
                float bx = (float)x;
                float by = (float)(5 - y);
                // シェーダーと同じ乱数計算をCPUで行う
                float dot_val = bx * 12.9898f + by * 78.233f;
                float s = std::sin(dot_val);
                float n = s * 43758.5453f;
                n = n - std::floor(n);
                
                float cellW = (halfWidth_ * 2.0f) / 6.0f;
                float cellH = (halfHeight_ * 2.0f) / 6.0f;
                float offsetX = -halfWidth_ + cellW * 0.5f + cellW * x;
                float offsetY = -halfHeight_ + cellH * 0.5f + cellH * y;
                
                Transform ghostTransform;
                ghostTransform.scale = { cellW, cellH, 1.0f };
                ghostTransform.rotate = {0.0f, 0.0f, 0.0f};
                
                Vector3 basePos = position_;
                basePos.x += offsetX;
                basePos.y += offsetY;
                
                Material ghostMat = primitiveObj_->GetMaterial();
                ghostMat.dissolveThreshold = 0.0f; // ゴースト自体はディゾルブさせない
                
                if (dissolveProgress > n) {
                    // ディゾルブ時間を超えたブロックは上に飛んでいく
                    float timeSinceDissolve = (dissolveProgress - n) * deathDuration_;
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
                    primitiveObj_->DrawGhost(commandList, ghostTransform, ghostMat);
                }
            }
        }
    } else {
        primitiveObj_->Draw(commandList);
    }

    // ダッシュ波紋エフェクトの描画
    if (dashRingPrimitive_) {
        for (const auto& ring : dashRingParticles_) {
            if (ring.active) {
                float progress = ring.timer / ring.duration;
                // イージングで広がる
                float currentSize = ring.startSize + (ring.endSize - ring.startSize) * (1.0f - std::pow(1.0f - progress, 3.0f));
                
                Transform ringTransform;
                ringTransform.scale = { currentSize, currentSize, currentSize };
                ringTransform.rotate = ring.rotation;
                ringTransform.translate = ring.position;
                
                Material ringMat = dashRingPrimitive_->GetMaterial();
                // 色はダッシュ色(白や水色)にして、徐々に透明にする
                ringMat.color = { 0.5f, 0.8f, 1.0f, 1.0f - progress }; // 薄い水色
                ringMat.dissolveThreshold = 0.0f;
                
                dashRingPrimitive_->DrawGhost(commandList, ringTransform, ringMat);
            }
        }
    }

    // 砂埃パーティクルの描画
    for (const auto& dust : dustParticles_) {
        if (dust.active) {
            float progress = dust.timer / dust.duration;
            float currentSize = dust.startSize * (1.0f - progress); // 徐々に小さく
            
            Transform dustTransform;
            dustTransform.scale = { currentSize, currentSize, 1.0f };
            dustTransform.rotate = { 0.0f, 0.0f, progress * 10.0f }; // 少し回転
            dustTransform.translate = dust.position;
            
            Material dustMat = primitiveObj_->GetMaterial();
            dustMat.color = { 0.8f, 0.8f, 0.8f, 1.0f - progress }; // 白っぽいグレーで透明に
            dustMat.dissolveThreshold = 0.0f;
            
            primitiveObj_->DrawGhost(commandList, dustTransform, dustMat);
        }
    }

    // 紙吹雪パーティクルの描画
    for (const auto& confetti : confettiParticles_) {
        if (confetti.active) {
            Transform t;
            t.scale = { confetti.size, confetti.size, 1.0f };
            t.rotate = confetti.rotation;
            t.translate = confetti.position;
            
            Material mat = primitiveObj_->GetMaterial();
            mat.color = confetti.color;
            // フェードアウト (最後の20%の時間で)
            float fadeStart = confetti.duration * 0.8f;
            if (confetti.timer > fadeStart) {
                mat.color.w = 1.0f - ((confetti.timer - fadeStart) / (confetti.duration - fadeStart));
            }
            mat.dissolveThreshold = 0.0f;
            
            primitiveObj_->DrawGhost(commandList, t, mat);
        }
    }
}

void Player2D::DisplayImGui() {
#ifdef USE_IMGUI
    if (ImGui::TreeNode("Player2D")) {
        ImGui::DragFloat3("Position", &position_.x, 0.1f);
        ImGui::DragFloat3("Velocity", &velocity_.x, 0.1f);
        ImGui::DragFloat("Dash End Upward Vel", &dashEndUpwardVelocity_, 0.1f, 0.0f, 10.0f);
        ImGui::ColorEdit4("Normal Color", &colorNormal_.x);
        ImGui::ColorEdit4("Dashed Color", &colorDashed_.x);
        
        bool sizeChanged = false;
        if (ImGui::DragFloat("Half Width", &halfWidth_, 0.01f, 0.05f, 5.0f)) {
            sizeChanged = true;
        }
        if (ImGui::DragFloat("Half Height", &halfHeight_, 0.01f, 0.05f, 5.0f)) {
            sizeChanged = true;
        }
        if (sizeChanged && primitiveObj_) {
            primitiveObj_->SetScale({ halfWidth_ * 2.0f, halfHeight_ * 2.0f, 1.0f });
        }

        ImGui::TreePop();
    }
#endif
}


Player2D::AABB Player2D::GetAABB() const {
    return {
        position_.x - halfWidth_,   // left
        position_.y + halfHeight_,  // top
        position_.x + halfWidth_,   // right
        position_.y - halfHeight_   // bottom
    };
}

bool Player2D::CheckAABBCollision(const AABB& a, const AABB& b) {
    if (a.right < b.left || a.left > b.right) return false;
    if (a.top < b.bottom || a.bottom > b.top) return false;
    return true;
}

bool Player2D::CheckCollisionOBB(const OBB2D& obb1, const OBB2D& obb2, Vector3& outMTV) {
    // 今回はAABBのみを使用するため、簡易的にAABB判定にフォールバック（将来のためのプレースホルダ）
    AABB a = { obb1.center.x - obb1.extents.x, obb1.center.y + obb1.extents.y, obb1.center.x + obb1.extents.x, obb1.center.y - obb1.extents.y };
    AABB b = { obb2.center.x - obb2.extents.x, obb2.center.y + obb2.extents.y, obb2.center.x + obb2.extents.x, obb2.center.y - obb2.extents.y };
    if (!CheckAABBCollision(a, b)) return false;

    // AABB同士のMTV計算
    float overlapLeft = a.right - b.left;
    float overlapRight = b.right - a.left;
    float overlapTop = a.top - b.bottom;
    float overlapBottom = b.top - a.bottom;

    float minOverlap = (std::min)({overlapLeft, overlapRight, overlapTop, overlapBottom});
    
    if (minOverlap == overlapLeft) outMTV = { -overlapLeft, 0.0f, 0.0f };
    else if (minOverlap == overlapRight) outMTV = { overlapRight, 0.0f, 0.0f };
    else if (minOverlap == overlapTop) outMTV = { 0.0f, overlapTop, 0.0f };
    else outMTV = { 0.0f, -overlapBottom, 0.0f };
    
    return true;
}

void Player2D::HandleInput() {
    KeyboardInput* keyboard = KeyboardInput::GetInstance();

    isWallSliding_ = false;
    isWallClinging_ = false;

    // リフトの慣性猶予（コヨーテタイム）の更新
    float dt = TimeManager::GetInstance().GetDeltaTime();
    if (isOnMovingPlatform_) {
        // リフトが動いていれば最新の速度を記録し、猶予時間をリセット
        if (std::abs(platformVelocity_.x) > 0.01f || std::abs(platformVelocity_.y) > 0.01f) {
            recentPlatformVelocity_ = platformVelocity_;
            platformInertiaTimer_ = 0.1f; // コヨーテタイムを0.1秒に変更
        }
    }
    if (platformInertiaTimer_ > 0.0f) {
        platformInertiaTimer_ -= dt;
    }

    // 通常時の左右移動（ダッシュ中でない場合）
    if (!isDashing_) {
        bool inputLeft = keyboard->IsKeyDown(DIK_A) || keyboard->IsKeyDown(DIK_LEFT);
        bool inputRight = keyboard->IsKeyDown(DIK_D) || keyboard->IsKeyDown(DIK_RIGHT);

        if (wallJumpTimer_ <= 0.0f) {
            float targetVelX = 0.0f;
            if (inputLeft) {
                targetVelX = -moveSpeed_;
            }
            if (inputRight) {
                targetVelX = moveSpeed_;
            }

            // 外部速度(慣性)の加算と減衰
            if (isOnGround_) {
                externalVelocityX_ = 0.0f; // 地上にいるときは慣性をリセット
            } else {
                // 空中では緩やかに減衰（空気抵抗）
                float dt = TimeManager::GetInstance().GetDeltaTime();
                float decayRate = 5.0f * dt;
                if (externalVelocityX_ > 0.0f) {
                    externalVelocityX_ -= decayRate;
                    if (externalVelocityX_ < 0.0f) externalVelocityX_ = 0.0f;
                } else if (externalVelocityX_ < 0.0f) {
                    externalVelocityX_ += decayRate;
                    if (externalVelocityX_ > 0.0f) externalVelocityX_ = 0.0f;
                }
            }

            velocity_.x = targetVelX + externalVelocityX_;
        }

        // 壁ずり落ち / 壁張り付きの判定 (空中で、落下中か静止中の場合のみ)
        if (!isOnGround_ && velocity_.y <= 0.0f) {
            if ((isTouchingWallRight_ && inputRight) || (isTouchingWallLeft_ && inputLeft)) {
                isWallSliding_ = true;
                if (keyboard->IsKeyDown(DIK_LCONTROL) || keyboard->IsKeyDown(DIK_RCONTROL)) {
                    isWallClinging_ = true;
                }
            }
        }

        // ジャンプ
        if (keyboard->IsKeyPressed(DIK_SPACE)) {
            if (isOnGround_) {
                velocity_.y = jumpPower_;
                // 足場に乗っている（または猶予期間中）場合は慣性を加算
                if (platformInertiaTimer_ > 0.0f) {
                    externalVelocityX_ = recentPlatformVelocity_.x;
                    velocity_.x += externalVelocityX_;
                    velocity_.y += recentPlatformVelocity_.y;
                    
                    // ジャンプしたら猶予期間を終了する
                    platformInertiaTimer_ = 0.0f;
                }
                isOnGround_ = false;
                isOnMovingPlatform_ = false;
                SpawnJumpDust({position_.x, position_.y - halfHeight_, 0.0f}, 0.0f);
            } else if (isTouchingWallRight_) {
                // 壁張り付き状態（Control入力がある場合）は真上ジャンプを優先
                bool isPressingCling = keyboard->IsKeyDown(DIK_LCONTROL) || keyboard->IsKeyDown(DIK_RCONTROL);
                if (isWallClinging_ || isPressingCling) {
                    // 壁張り付き中は真上ジャンプ
                    velocity_.x = 0.0f;
                    velocity_.y = jumpPower_;
                } else {
                    // 右壁キック（左へ跳ね返る）
                    velocity_.x = -wallJumpPower_.x;
                    velocity_.y = wallJumpPower_.y;
                    wallJumpTimer_ = wallJumpDuration_;
                    externalVelocityX_ = 0.0f; // 壁ジャンプ時に慣性をリセット
                    SpawnJumpDust({position_.x + halfWidth_, position_.y, 0.0f}, -1.0f);
                }
                isTouchingWallRight_ = false;
                isWallSliding_ = false;
                isWallClinging_ = false;
            } else if (isTouchingWallLeft_) {
                // 壁張り付き状態（Control入力がある場合）は真上ジャンプを優先
                bool isPressingCling = keyboard->IsKeyDown(DIK_LCONTROL) || keyboard->IsKeyDown(DIK_RCONTROL);
                if (isWallClinging_ || isPressingCling) {
                    // 壁張り付き中は真上ジャンプ
                    velocity_.x = 0.0f;
                    velocity_.y = jumpPower_;
                } else {
                    // 左壁キック（右へ跳ね返る）
                    velocity_.x = wallJumpPower_.x;
                    velocity_.y = wallJumpPower_.y;
                    wallJumpTimer_ = wallJumpDuration_;
                    externalVelocityX_ = 0.0f; // 壁ジャンプ時に慣性をリセット
                    SpawnJumpDust({position_.x - halfWidth_, position_.y, 0.0f}, 1.0f);
                }
                isTouchingWallLeft_ = false;
                isWallSliding_ = false;
                isWallClinging_ = false;
            }
        }
    }

    // ダッシュの入力検知（SHIFTキー）
    if (canDash_ && !isDashing_ && (keyboard->IsKeyPressed(DIK_LSHIFT) || keyboard->IsKeyPressed(DIK_RSHIFT))) {
        // 入力方向の取得
        Vector3 inputDir = {0.0f, 0.0f, 0.0f};
        if (keyboard->IsKeyDown(DIK_A) || keyboard->IsKeyDown(DIK_LEFT)) inputDir.x -= 1.0f;
        if (keyboard->IsKeyDown(DIK_D) || keyboard->IsKeyDown(DIK_RIGHT)) inputDir.x += 1.0f;
        if (keyboard->IsKeyDown(DIK_W) || keyboard->IsKeyDown(DIK_UP)) inputDir.y += 1.0f;
        if (keyboard->IsKeyDown(DIK_S) || keyboard->IsKeyDown(DIK_DOWN)) inputDir.y -= 1.0f;

        // 入力が無い場合は向いている方向にするなどの処理が必要だが、とりあえず右とする
        if (inputDir.x == 0.0f && inputDir.y == 0.0f) {
            inputDir.x = 1.0f;
        } else {
            inputDir = TransformFunctions::Normalize(inputDir);
        }

        dashVelocity_ = { inputDir.x * dashSpeed_, inputDir.y * dashSpeed_, 0.0f };
        velocity_ = dashVelocity_;
        isDashing_ = true;
        canDash_ = false;
        dashTimer_ = 0.0f;
        
        // ダッシュ波紋を発生
        SpawnDashRing(position_, inputDir);
    }
}

void Player2D::ApplyGravity(float deltaTime) {
    if (isDashing_) return; // ダッシュ中は重力を無視

    if (isWallClinging_) {
        velocity_.y = 0.0f; // 張り付き中は落下しない
        return;
    }

    velocity_.y += gravity_ * deltaTime;

    // 最大落下速度を制限
    float currentMaxFallSpeed = maxFallSpeed_;
    if (isWallSliding_ && velocity_.y < 0.0f) {
        currentMaxFallSpeed = wallSlideSpeed_; // ずり落ち中はゆっくり落下
    }

    if (velocity_.y < currentMaxFallSpeed) {
        velocity_.y = currentMaxFallSpeed;
    }
}

void Player2D::ResolveCollisionY(const MapChip2D& map) {
    float chipSize = map.GetChipSize();
    isOnGround_ = false;
    isOnMovingPlatform_ = false;
    platformVelocity_ = {0.0f, 0.0f, 0.0f};

    AABB aabb = GetAABB();

    // 足元・頭上のチップ範囲を調べる (少し内側を調べて壁滑りをよくする)
    int leftChip = map.WorldToChipX(aabb.left + 0.05f);
    int rightChip = map.WorldToChipX(aabb.right - 0.05f);

    if (velocity_.y <= 0.0f) {
        // 下方向：足元チェック
        int bottomChip = map.WorldToChipY(aabb.bottom);
        for (int cx = leftChip; cx <= rightChip; ++cx) {
            bool isBlock = false;
            bool isOneWay = false;
            BaseBlock* block = map.GetBlock(cx, bottomChip);
            if (block) {
                isBlock = block->IsSolid();
                isOneWay = block->IsOneWay();
            } else if (cx < 0 || cx >= map.GetWidth() || bottomChip < 0) {
                isBlock = true; // 範囲外（左右下）は壁・床扱い
            }
            
            if (isBlock || isOneWay) {
                float blockTop = map.ChipToWorldY(bottomChip) + chipSize;
                
                if (isOneWay) {
                    // 移動前の足元座標を計算
                    float previousBottom = position_.y - velocity_.y * TimeManager::GetInstance().GetDeltaTime() - halfHeight_;
                    // もし前のフレームでブロックの上面より下にいた場合はすり抜ける (少しマージンを持たせる)
                    if (previousBottom < blockTop - 0.05f) {
                        continue;
                    }
                }

                // 地面の上に押し戻す
                position_.y = blockTop + halfHeight_;
                velocity_.y = 0.0f;
                isOnGround_ = true;
                canDash_ = true; // 着地でダッシュ回復
                
                if (block) {
                    block->OnPlayerStand();
                    if (block->IsMoving()) {
                        isOnMovingPlatform_ = true;
                        platformVelocity_ = block->GetVelocity();
                    }
                }
                
                break;
            }
        }
    } else {
        // 上方向：頭上チェック
        int topChip = map.WorldToChipY(aabb.top);
        for (int cx = leftChip; cx <= rightChip; ++cx) {
            bool isBlock = false;
            BaseBlock* block = map.GetBlock(cx, topChip);
            if (block) {
                isBlock = block->IsSolid();
            } else if (cx < 0 || cx >= map.GetWidth()) {
                isBlock = true;
            }
            // 上方向への範囲外は空気扱いとする
            if (isBlock) {
                float blockBottom = map.ChipToWorldY(topChip);
                position_.y = blockBottom - halfHeight_;
                velocity_.y = 0.0f;
                break;
            }
        }
    }

    // 動的ブロック（リフトなど）の判定
    aabb = GetAABB(); // 静的ブロックで位置が変わった可能性があるので再取得
    
    // 壁との擦れ判定を防ぐため、左右を少しだけ縮める
    AABB shrunkAABBY = aabb;
    shrunkAABBY.left += 0.05f;
    shrunkAABBY.right -= 0.05f;

    const auto& updateBlocks = map.GetUpdateBlocks();
    for (const auto& block : updateBlocks) {
        if (!block || !block->IsSolid()) continue;
        
        PrimitiveObject* pObj = block->GetPrimitive();
        if (!pObj) continue;
        
        Vector3 pos = pObj->GetTranslation();
        Vector3 scale = pObj->GetScale();
        
        AABB blockAABB = {
            pos.x - scale.x * 0.5f,
            pos.y + scale.y * 0.5f,
            pos.x + scale.x * 0.5f,
            pos.y - scale.y * 0.5f
        };

        if (CheckAABBCollision(shrunkAABBY, blockAABB)) {
            if (velocity_.y <= 0.0f && aabb.bottom >= blockAABB.top - 0.5f) { // 上から乗った
                position_.y = blockAABB.top + halfHeight_;
                velocity_.y = 0.0f;
                isOnGround_ = true;
                canDash_ = true;
                block->OnPlayerStand();
                if (block->IsMoving()) {
                    isOnMovingPlatform_ = true;
                    platformVelocity_ = block->GetVelocity();
                }
            } else if (velocity_.y > 0.0f && aabb.top <= blockAABB.bottom + 0.5f) { // 下からぶつかった
                position_.y = blockAABB.bottom - halfHeight_;
                velocity_.y = 0.0f;
            }
        }
    }
}

void Player2D::ResolveCollisionX(const MapChip2D& map) {
    float chipSize = map.GetChipSize();

    isTouchingWallLeft_ = false;
    isTouchingWallRight_ = false;

    AABB aabb = GetAABB();

    // 左右のチップ範囲を調べる (少し内側を調べて段差に引っかかりにくくする)
    int topChip = map.WorldToChipY(aabb.top - 0.05f);
    int bottomChip = map.WorldToChipY(aabb.bottom + 0.05f);

    if (velocity_.x > 0.0f) {
        // 右方向
        int rightChip = map.WorldToChipX(aabb.right);
        for (int cy = bottomChip; cy <= topChip; ++cy) {
            bool isBlock = false;
            BaseBlock* block = map.GetBlock(rightChip, cy);
            if (block) {
                isBlock = block->IsSolid();
            } else if (rightChip >= map.GetWidth() || cy < 0) {
                isBlock = true;
            }

            if (isBlock) {
                float blockLeft = map.ChipToWorldX(rightChip);
                position_.x = blockLeft - halfWidth_;
                velocity_.x = 0.0f;
                isTouchingWallRight_ = true;
                break;
            }
        }
    } else if (velocity_.x < 0.0f) {
        // 左方向
        int leftChip = map.WorldToChipX(aabb.left);
        for (int cy = bottomChip; cy <= topChip; ++cy) {
            bool isBlock = false;
            BaseBlock* block = map.GetBlock(leftChip, cy);
            if (block) {
                isBlock = block->IsSolid();
            } else if (leftChip < 0 || cy < 0) {
                isBlock = true;
            }

            if (isBlock) {
                float blockRight = map.ChipToWorldX(leftChip) + chipSize;
                position_.x = blockRight + halfWidth_;
                velocity_.x = 0.0f;
                isTouchingWallLeft_ = true;
                break;
            }
        }
    }

    // 動的ブロック（リフトなど）の判定
    aabb = GetAABB();
    
    // 床や天井との擦れ判定を防ぐため、上下を少しだけ縮める
    AABB shrunkAABBX = aabb;
    shrunkAABBX.top -= 0.05f;
    shrunkAABBX.bottom += 0.05f;

    for (const auto& block : map.GetUpdateBlocks()) {
        if (!block || !block->IsSolid()) continue;
        
        PrimitiveObject* pObj = block->GetPrimitive();
        if (!pObj) continue;
        
        Vector3 pos = pObj->GetTranslation();
        Vector3 scale = pObj->GetScale();
        
        AABB blockAABB = {
            pos.x - scale.x * 0.5f,
            pos.y + scale.y * 0.5f,
            pos.x + scale.x * 0.5f,
            pos.y - scale.y * 0.5f
        };

        if (CheckAABBCollision(shrunkAABBX, blockAABB)) {
            // ブロックの中心とプレイヤーの中心を比較して左右を判定
            if (position_.x < pos.x) {
                // ブロックの左側にいる
                position_.x = blockAABB.left - halfWidth_;
                velocity_.x = 0.0f;
                isTouchingWallRight_ = true;
            } else {
                // ブロックの右側にいる
                position_.x = blockAABB.right + halfWidth_;
                velocity_.x = 0.0f;
                isTouchingWallLeft_ = true;
            }
        }
    }
}

void Player2D::SpawnJumpDust(const Vector3& basePos, float dirX) {
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

void Player2D::SpawnRunDust(const Vector3& basePos, float dirX) {
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

void Player2D::SpawnConfetti() {
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
        confetti.position = position_;
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

float Player2D::EaseInElastic(float t) const {
    const float c4 = (2.0f * 3.14159265f) / 3.0f;
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;
    return -std::pow(2.0f, 10.0f * t - 10.0f) * std::sin((t * 10.0f - 10.75f) * c4);
}

void Player2D::SimulateCollisions(MapChip2D& map) {
    AABB aabb = GetAABB();
    // 押し戻しによって境界線上に位置した際も検知できるよう、わずかなマージンを持たせる
    const float margin = 0.02f;
    int leftChip = map.WorldToChipX(aabb.left - margin);
    int rightChip = map.WorldToChipX(aabb.right + margin);
    int bottomChip = map.WorldToChipY(aabb.bottom - margin);
    int topChip = map.WorldToChipY(aabb.top + margin);

    for (int cy = bottomChip; cy <= topChip; ++cy) {
        for (int cx = leftChip; cx <= rightChip; ++cx) {
            BaseBlock* block = map.GetBlock(cx, cy);
            if (block) {
                block->OnCollision(this);
            }
        }
    }
}

void Player2D::SpawnDashRing(const Vector3& basePos, const Vector3& dashDir) {
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
