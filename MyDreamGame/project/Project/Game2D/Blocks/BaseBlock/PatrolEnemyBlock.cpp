#include "PatrolEnemyBlock.h"
#include "Game2D/Player/Player2D.h"
#include "Game2D/MapChip2D.h"
#include "Resource/Primitive/PrimitiveManager.h"
#include "Graphics/TextureManager.h"
#include "Core/TimeManager.h"
#include "Editor/Replay/ReplayManager.h"
#ifdef USE_IMGUI
#include "Editor/EditorManager.h"
#endif

void PatrolEnemyBlock::Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) {
    SetGameObject(std::make_unique<GameObject>("PatrolEnemy"));
    auto *tc = GetGameObject()->AddComponent<TransformComponent>();
    auto *prc = GetGameObject()->AddComponent<PrimitiveRendererComponent>();

    Primitive* spherePrimitive = PrimitiveManager::GetInstance()->GetPrimitive(PrimitiveType::Sphere, 1.0f);
    prc->Initialize(device, spherePrimitive ? spherePrimitive : boxPrimitive);

    uint32_t texHandle = TextureManager::GetInstance()->Load("resources/Object/Original/enemy/monsterBall.png");
    if (texHandle != 0) {
        prc->SetTextureHandle(TextureManager::GetInstance()->GetGpuHandle(texHandle));
    }
    prc->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
    prc->SetLightingType(1);
    prc->SetEnableBoxMapping(0.0f);
    prc->SetUVTransform(TransformFunctions::MakeIdentity4x4());

    radius_ = width * 0.5f;
    if (radius_ <= 0.0f) radius_ = 0.5f;

    currentPos_ = { worldX, worldY, 0.0f };
    startPos_ = currentPos_;
    velocity_ = { 0.0f, 0.0f, 0.0f };
    moveDir_ = 1.0f;
    rollAngle_ = 0.0f;
    isOnGround_ = false;

    tc->SetScale({ radius_ * 2.0f, radius_ * 2.0f, radius_ * 2.0f });
    tc->SetPosition(currentPos_);
    tc->SetRotation({ 0.0f, 1.57079632f, 0.0f }); // ボタンを正面（カメラ側）に向ける

    SetupCollider();
}

void PatrolEnemyBlock::Update() {
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
    if (!isAnimActive) {
        // Playしていない時は初期位置で停止して描画のみ更新
        currentPos_ = startPos_;
        velocity_ = { 0.0f, 0.0f, 0.0f };
        moveDir_ = 1.0f;
        rollAngle_ = 0.0f;
        isOnGround_ = false;
        if (GetGameObject()) {
            if (auto *tc = GetGameObject()->GetComponent<TransformComponent>()) {
                tc->SetPosition(startPos_);
                tc->SetRotation({ 0.0f, 1.57079632f, 0.0f });
            }
            GetGameObject()->Update();
        }
        return;
    }

    float dt = TimeManager::GetInstance().GetDeltaTime();
    if (dt <= 0.0f) {
        if (GetGameObject())
            GetGameObject()->Update();
        return;
    }

    if (GetMap()) {
        // 1. 壁との衝突検知（進行方向にSolidブロックがあるか）
        float wallCheckX = currentPos_.x + moveDir_ * (radius_ + 0.1f);
        int wallChipX = GetMap()->WorldToChipX(wallCheckX);
        int chipY = GetMap()->WorldToChipY(currentPos_.y);

        if (wallChipX < 0 || wallChipX >= GetMap()->GetWidth()) {
            moveDir_ = -moveDir_;
        } else {
            BaseBlock *wallBlock = GetMap()->GetBlock(wallChipX, chipY);
            if (wallBlock && wallBlock->IsSolid()) {
                moveDir_ = -moveDir_;
            }
        }

        // 2. 崖（床抜け）検知（進行方向の足元に床があるか）
        if (isOnGround_) {
            float floorCheckX = currentPos_.x + moveDir_ * (radius_ * 0.8f);
            float floorCheckY = currentPos_.y - radius_ - 0.2f;
            int floorChipX = GetMap()->WorldToChipX(floorCheckX);
            int floorChipY = GetMap()->WorldToChipY(floorCheckY);

            bool hasFloor = false;
            if (floorChipX >= 0 && floorChipX < GetMap()->GetWidth() && floorChipY >= 0 && floorChipY < GetMap()->GetHeight()) {
                BaseBlock *floorBlock = GetMap()->GetBlock(floorChipX, floorChipY);
                if (floorBlock && floorBlock->IsSolid()) {
                    hasFloor = true;
                }
            }

            if (!hasFloor) {
                moveDir_ = -moveDir_; // 床が無いので反転
            }
        }

        // 3. 重力と地面接地判定
        float nextY = currentPos_.y + velocity_.y * dt;
        int underChipX = GetMap()->WorldToChipX(currentPos_.x);
        int underChipY = GetMap()->WorldToChipY(nextY - radius_);

        isOnGround_ = false;
        if (underChipX >= 0 && underChipX < GetMap()->GetWidth() && underChipY >= 0 && underChipY < GetMap()->GetHeight()) {
            BaseBlock *underBlock = GetMap()->GetBlock(underChipX, underChipY);
            if (underBlock && underBlock->IsSolid()) {
                float blockTopY = GetMap()->ChipToWorldY(underChipY) + GetMap()->GetChipSize(); // ブロックの上面（底辺 + 高さ）
                if (nextY - radius_ <= blockTopY && velocity_.y <= 0.0f) {
                    currentPos_.y = blockTopY + radius_;
                    velocity_.y = 0.0f;
                    isOnGround_ = true;
                }
            }
        }

        if (!isOnGround_) {
            velocity_.y += gravity_ * dt;
            currentPos_.y += velocity_.y * dt;
        }
    }

    // 4. 移動と回転の反映
    velocity_.x = moveSpeed_ * moveDir_;
    currentPos_.x += velocity_.x * dt;
    rollAngle_ -= (velocity_.x / radius_) * dt;

    if (GetGameObject()) {
        if (auto *tc = GetGameObject()->GetComponent<TransformComponent>()) {
            tc->SetPosition(currentPos_);
            tc->SetRotation({ 0.0f, 1.57079632f, rollAngle_ });
        }
        GetGameObject()->Update();
    }
}

void PatrolEnemyBlock::OnCollision(Player2D* player) {
    if (player) {
        player->Kill();
    }
}

void PatrolEnemyBlock::SetProperties(const nlohmann::json& properties) {
    if (properties.contains("moveSpeed")) {
        moveSpeed_ = properties["moveSpeed"].get<float>();
    }
}

void PatrolEnemyBlock::Reset() {
    currentPos_ = startPos_;
    velocity_ = { 0.0f, 0.0f, 0.0f };
    moveDir_ = 1.0f;
    rollAngle_ = 0.0f;
    isOnGround_ = false;
    if (GetGameObject()) {
        if (auto *tc = GetGameObject()->GetComponent<TransformComponent>()) {
            tc->SetPosition(currentPos_);
            tc->SetRotation({ 0.0f, 1.57079632f, 0.0f });
        }
    }
}
