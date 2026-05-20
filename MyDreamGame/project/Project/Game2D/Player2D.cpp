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
    uint32_t texHandle = TextureManager::GetInstance()->Load("Object/School/human/white.png", comPtrCommandList);
    primitiveObj_->SetTextureHandle(TextureManager::GetInstance()->GetGpuHandle(texHandle));

    // プレイヤーの見た目設定
    primitiveObj_->SetScale({ halfWidth_ * 2.0f, halfHeight_ * 2.0f, 1.0f });
    primitiveObj_->GetMaterial().color = colorNormal_; // 青色
    primitiveObj_->GetMaterial().lightingType = 0; // ライティング無効（2Dなので）
}

void Player2D::Update(const MapChip2D& map) {
    float deltaTime = TimeManager::GetInstance().GetDeltaTime();

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
    position_.x += velocity_.x * deltaTime;
    ResolveCollisionX(map);

    // 画面外落下時のリスポーン
    if (position_.y < -10.0f) {
        position_ = { 2.0f, 5.0f, 0.0f };
        velocity_ = { 0.0f, 0.0f, 0.0f };
        isDashing_ = false;
        canDash_ = true;
    }

    // 色の更新
    primitiveObj_->GetMaterial().color = canDash_ ? colorNormal_ : colorDashed_;

    // PrimitiveObjectの座標を更新
    primitiveObj_->SetTranslation(position_);
    primitiveObj_->Update();
}

void Player2D::Draw(ID3D12GraphicsCommandList* commandList) {
    primitiveObj_->Draw(commandList);
}

void Player2D::DisplayImGui() {
#ifdef USE_IMGUI
    if (ImGui::TreeNode("Player2D Settings")) {
        ImGui::Text("--- Physics & Position ---");
        ImGui::DragFloat3("Position", &position_.x, 0.1f);
        ImGui::DragFloat3("Velocity", &velocity_.x, 0.1f);
        ImGui::Checkbox("On Ground", &isOnGround_);

        ImGui::Text("--- Movement ---");
        ImGui::DragFloat("Move Speed", &moveSpeed_, 0.1f, 0.0f, 50.0f);
        ImGui::DragFloat("Jump Power", &jumpPower_, 0.1f, 0.0f, 50.0f);
        ImGui::DragFloat("Gravity", &gravity_, 0.1f, -100.0f, 0.0f);
        ImGui::DragFloat("Max Fall Speed", &maxFallSpeed_, 0.1f, -100.0f, 0.0f);

        ImGui::Text("--- Dash Settings ---");
        ImGui::Checkbox("Can Dash", &canDash_);
        ImGui::Checkbox("Is Dashing", &isDashing_);
        ImGui::DragFloat("Dash Speed", &dashSpeed_, 0.1f, 0.0f, 100.0f);
        ImGui::DragFloat("Dash Duration", &dashDuration_, 0.01f, 0.0f, 2.0f);

        ImGui::Text("--- Wall Action Settings ---");
        ImGui::DragFloat("Wall Slide Speed", &wallSlideSpeed_, 0.1f, -50.0f, 0.0f);
        ImGui::DragFloat("Wall Jump Duration", &wallJumpDuration_, 0.01f, 0.0f, 2.0f);
        ImGui::DragFloat2("Wall Jump Power (X, Y)", &wallJumpPower_.x, 0.1f, 0.0f, 50.0f);
        
        ImGui::Text("--- Visuals & Size ---");
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

void Player2D::HandleInput() {
    KeyboardInput* keyboard = KeyboardInput::GetInstance();

    isWallSliding_ = false;
    isWallClinging_ = false;

    // 通常時の左右移動（ダッシュ中でない場合）
    if (!isDashing_) {
        bool inputLeft = keyboard->IsKeyDown(DIK_A) || keyboard->IsKeyDown(DIK_LEFT);
        bool inputRight = keyboard->IsKeyDown(DIK_D) || keyboard->IsKeyDown(DIK_RIGHT);

        if (wallJumpTimer_ <= 0.0f) {
            velocity_.x = 0.0f;
            if (inputLeft) {
                velocity_.x = -moveSpeed_;
            }
            if (inputRight) {
                velocity_.x = moveSpeed_;
            }
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
                isOnGround_ = false;
            } else if (isTouchingWallRight_) {
                // 壁張り付き状態、または壁方向への入力・Control入力がある場合は真上ジャンプを優先
                bool isPressingCling = keyboard->IsKeyDown(DIK_LCONTROL) || keyboard->IsKeyDown(DIK_RCONTROL);
                if (isWallSliding_ || isWallClinging_ || inputRight || isPressingCling) {
                    // 壁張り付き/ずり落ち中は真上ジャンプ
                    velocity_.x = 0.0f;
                    velocity_.y = jumpPower_;
                } else {
                    // 右壁キック（左へ跳ね返る）
                    velocity_.x = -wallJumpPower_.x;
                    velocity_.y = wallJumpPower_.y;
                    wallJumpTimer_ = wallJumpDuration_;
                }
                isTouchingWallRight_ = false;
                isWallSliding_ = false;
                isWallClinging_ = false;
            } else if (isTouchingWallLeft_) {
                // 壁張り付き状態、または壁方向への入力・Control入力がある場合は真上ジャンプを優先
                bool isPressingCling = keyboard->IsKeyDown(DIK_LCONTROL) || keyboard->IsKeyDown(DIK_RCONTROL);
                if (isWallSliding_ || isWallClinging_ || inputLeft || isPressingCling) {
                    // 壁張り付き/ずり落ち中は真上ジャンプ
                    velocity_.x = 0.0f;
                    velocity_.y = jumpPower_;
                } else {
                    // 左壁キック（右へ跳ね返る）
                    velocity_.x = wallJumpPower_.x;
                    velocity_.y = wallJumpPower_.y;
                    wallJumpTimer_ = wallJumpDuration_;
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

    AABB aabb = GetAABB();

    // 足元・頭上のチップ範囲を調べる (少し内側を調べて壁滑りをよくする)
    int leftChip = map.WorldToChipX(aabb.left + 0.05f);
    int rightChip = map.WorldToChipX(aabb.right - 0.05f);

    if (velocity_.y <= 0.0f) {
        // 下方向：足元チェック
        int bottomChip = map.WorldToChipY(aabb.bottom);
        for (int cx = leftChip; cx <= rightChip; ++cx) {
            if (map.IsBlock(cx, bottomChip)) {
                // 地面の上に押し戻す
                float blockTop = map.ChipToWorldY(bottomChip) + chipSize;
                position_.y = blockTop + halfHeight_;
                velocity_.y = 0.0f;
                isOnGround_ = true;
                canDash_ = true; // 着地でダッシュ回復
                break;
            }
        }
    } else {
        // 上方向：頭上チェック
        int topChip = map.WorldToChipY(aabb.top);
        for (int cx = leftChip; cx <= rightChip; ++cx) {
            if (map.IsBlock(cx, topChip)) {
                float blockBottom = map.ChipToWorldY(topChip);
                position_.y = blockBottom - halfHeight_;
                velocity_.y = 0.0f;
                break;
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
            if (map.IsBlock(rightChip, cy)) {
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
            if (map.IsBlock(leftChip, cy)) {
                float blockRight = map.ChipToWorldX(leftChip) + chipSize;
                position_.x = blockRight + halfWidth_;
                velocity_.x = 0.0f;
                isTouchingWallLeft_ = true;
                break;
            }
        }
    }
}
