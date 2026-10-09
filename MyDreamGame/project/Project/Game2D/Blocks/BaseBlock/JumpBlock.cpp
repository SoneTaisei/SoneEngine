#include "JumpBlock.h"
#include "Game2D/Player/Player2D.h"
#include "Game2D/MapChip2D.h"
#include "Core/TimeManager.h"
#include <cmath>
#include <algorithm>
#ifdef USE_IMGUI
#include "Editor/EditorManager.h"
#include <imgui.h>
#endif
#include "Editor/Replay/ReplayManager.h"

JumpBlock::BounceDirection JumpBlock::DetermineFacingDirection() const {
    if (!map_) return BounceDirection::kUp;

    // 周囲のブロック状況を取得
    bool hasRight  = map_->GetBlock(chipX_ + 1, chipY_) != nullptr;
    bool hasLeft   = map_->GetBlock(chipX_ - 1, chipY_) != nullptr;
    bool hasTop    = map_->GetBlock(chipX_, chipY_ + 1) != nullptr;
    bool hasBottom = map_->GetBlock(chipX_, chipY_ - 1) != nullptr;

    bool isFloating = (!hasRight && !hasLeft && !hasTop && !hasBottom);

    if (hasBottom) {
        return BounceDirection::kUp; // 下にブロックがあるなら上面で跳ねる（上向き）
    } else if (hasLeft) {
        return BounceDirection::kRight; // 左にブロックがあるなら右面で跳ねる（右向き）
    } else if (hasRight) {
        return BounceDirection::kLeft; // 右にブロックがあるなら左面で跳ねる（左向き）
    } else if (hasTop) {
        return BounceDirection::kDown; // 上にブロックがあるなら下面で跳ねる（下向き）
    } else {
        return isFloating ? BounceDirection::kCenter : BounceDirection::kUp; // 完全に浮いている場合
    }
}

float JumpBlock::GetRotationZForDirection(BounceDirection dir) const {
    switch (dir) {
    case BounceDirection::kUp:
    case BounceDirection::kCenter:
        return 0.0f;
    case BounceDirection::kRight:
        return -1.570796327f; // -π/2 (時計回り90度)
    case BounceDirection::kLeft:
        return 1.570796327f;  // +π/2 (反時計回り90度)
    case BounceDirection::kDown:
        return 3.141592654f;  // π (180度)
    default:
        return 0.0f;
    }
}

void JumpBlock::Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) {
    gameObject_ = std::make_unique<GameObject>("JumpBlock");
    auto* tc = gameObject_->AddComponent<TransformComponent>();
    auto* prc = gameObject_->AddComponent<PrimitiveRendererComponent>();

    prc->Initialize(device, boxPrimitive);
    
    // ジャンプ台の色：オレンジ色
    prc->GetMaterial().color = { 1.0f, 0.5f, 0.0f, 1.0f };
    
    BounceDirection initialDir = DetermineFacingDirection();
    baseRotation_ = { 0.0f, 0.0f, GetRotationZForDirection(initialDir) };
    tc->SetRotation(baseRotation_);
    tc->SetScale({ width, height, 1.0f });
    tc->SetPosition({ worldX, worldY, 0.0f });
    prc->GetMaterial().lightingType = 1; // ライティング無効化
    SetupCollider();

    basePosition_ = { worldX, worldY, 0.0f };
    baseScale_ = { width, height, 1.0f };
    baseAABB_ = {
        basePosition_.x - baseScale_.x * 0.5f,
        basePosition_.y + baseScale_.y * 0.5f,
        basePosition_.x + baseScale_.x * 0.5f,
        basePosition_.y - baseScale_.y * 0.5f
    };
    if (auto* cc = gameObject_->GetComponent<ColliderComponent>()) {
        cc->SetCustomAABB(baseAABB_);
    }
    baseCaptured_ = true;
}

void JumpBlock::Update() {
    BaseBlock::Update();

    if (!gameObject_) return;
    auto* tc = gameObject_->GetComponent<TransformComponent>();
    if (!tc) return;

    // バウンドしていない通常時は、現在のTransformをベースとして同期（エディタによる移動やカスタムパレットのスケール変更に対応）
    if (!isBouncing_) {
        BounceDirection facing = DetermineFacingDirection();
        baseRotation_ = { 0.0f, 0.0f, GetRotationZForDirection(facing) };
        tc->SetRotation(baseRotation_);

        basePosition_ = tc->GetPosition();
        baseScale_ = tc->GetScale();
        baseAABB_ = {
            basePosition_.x - baseScale_.x * 0.5f,
            basePosition_.y + baseScale_.y * 0.5f,
            basePosition_.x + baseScale_.x * 0.5f,
            basePosition_.y - baseScale_.y * 0.5f
        };
        if (auto* cc = gameObject_->GetComponent<ColliderComponent>()) {
            cc->SetCustomAABB(baseAABB_);
        }
        baseCaptured_ = true;
        return;
    }

    // ゲーム再生中かつ非一時停止中のみアニメーションを進行
    float deltaTime = TimeManager::GetInstance().GetDeltaTime();
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
    if (!isAnimActive) return;

    // 物理スプリング計算（サブステップによる数値安定化）
    float dt = (std::min)(deltaTime, 0.05f);
    const int subSteps = 2;
    float subDt = dt / static_cast<float>(subSteps);

    for (int i = 0; i < subSteps; ++i) {
        float force = -stiffness_ * bounceAmount_ - damping_ * bounceVelocity_;
        bounceVelocity_ += force * subDt;
        bounceAmount_ += bounceVelocity_ * subDt;
    }

    // 収束判定
    if (std::abs(bounceAmount_) < 0.001f && std::abs(bounceVelocity_) < 0.005f) {
        bounceAmount_ = 0.0f;
        bounceVelocity_ = 0.0f;
        isBouncing_ = false;

        tc->SetPosition(basePosition_);
        tc->SetScale(baseScale_);
        tc->SetRotation(baseRotation_);
        return;
    }

    // スカッシュ＆ストレッチ変形率（主軸と垂直副軸）
    float mainFactor = 1.0f + bounceAmount_;
    mainFactor = (std::max)(0.2f, mainFactor);
    float crossFactor = 1.0f - bounceAmount_ * crossScaleRatio_;
    crossFactor = (std::max)(0.2f, crossFactor);

    // どの向きでもローカルYがばねの伸縮主軸、ローカルX（とZ）が副軸
    Vector3 currentScale = baseScale_;
    currentScale.y = baseScale_.y * mainFactor;
    currentScale.x = baseScale_.x * crossFactor;

    Vector3 currentPos = basePosition_;
    float lengthDelta = (currentScale.y - baseScale_.y) * 0.5f;

    switch (bounceDir_) {
    case BounceDirection::kUp: {
        // 底面（下）を固定し、上(+Y)へ伸びる
        currentPos.y = basePosition_.y + lengthDelta;
        break;
    }
    case BounceDirection::kDown: {
        // 上面（上）を固定し、下(-Y)へ伸びる
        currentPos.y = basePosition_.y - lengthDelta;
        break;
    }
    case BounceDirection::kLeft: {
        // 右面（右）を固定し、左(-X)へ伸びる
        currentPos.x = basePosition_.x - lengthDelta;
        break;
    }
    case BounceDirection::kRight: {
        // 左面（左）を固定し、右(+X)へ伸びる
        currentPos.x = basePosition_.x + lengthDelta;
        break;
    }
    case BounceDirection::kCenter:
    default: {
        // 浮遊：中心基準で伸縮（currentPosはbasePosition_のまま）
        break;
    }
    }

    tc->SetPosition(currentPos);
    tc->SetScale(currentScale);
    tc->SetRotation(baseRotation_);
}

void JumpBlock::Reset() {
    BaseBlock::Reset();
    isBouncing_ = false;
    bounceAmount_ = 0.0f;
    bounceVelocity_ = 0.0f;
    if (gameObject_ && baseCaptured_) {
        if (auto* tc = gameObject_->GetComponent<TransformComponent>()) {
            tc->SetPosition(basePosition_);
            tc->SetScale(baseScale_);
            tc->SetRotation(baseRotation_);
        }
    }
}

AABB2D JumpBlock::GetAABB() const {
    if (baseCaptured_) {
        return baseAABB_;
    }
    return BaseBlock::GetAABB();
}

void JumpBlock::TriggerBounce(BounceDirection dir) {
    bounceDir_ = dir;
    isBouncing_ = true;
    bounceAmount_ = initialSquash_;
    bounceVelocity_ = initialVelocity_;
}

void JumpBlock::OnPlayerStand() {
    // プレイヤーが乗った時に処理される可能性があるが、
    // 確実に処理するために OnCollision でも判定を行うのが安全
}

void JumpBlock::OnCollision(Player2D* player) {
    if (!player) return;

    // ジャンプ台の AABB を取得（アニメーション変形の影響を受けない固定AABB）
    AABB2D blockAABB = GetAABB();
    // プレイヤーの AABB を取得
    AABB2D playerAABB = player->GetAABB();
    
    // 設置向きを共通関数で判定し、モデルの向きと動作を完全に一致させる
    BounceDirection facingDir = DetermineFacingDirection();
    bool activeTop    = (facingDir == BounceDirection::kUp || facingDir == BounceDirection::kCenter);
    bool activeBottom = (facingDir == BounceDirection::kDown);
    bool activeRight  = (facingDir == BounceDirection::kRight);
    bool activeLeft   = (facingDir == BounceDirection::kLeft);
    bool isFloating   = (facingDir == BounceDirection::kCenter);

    // 各面との距離を計算（Player2D側でめり込みが押し戻されているため、接触面は距離がほぼ0になる）
    float distTop = std::abs(playerAABB.bottom - blockAABB.top);
    float distBottom = std::abs(playerAABB.top - blockAABB.bottom);
    float distLeft = std::abs(playerAABB.right - blockAABB.left);
    float distRight = std::abs(playerAABB.left - blockAABB.right);

    float minDist = (std::min)({ distTop, distBottom, distLeft, distRight });

    Vector3 vel = player->GetVelocity();
    const float threshold = 0.15f; // 接触判定の余裕幅

    bool isGrazeTop = distTop < 0.5f && vel.y <= 0.0f;

    if ((minDist == distTop && distTop < threshold && activeTop) || 
        ((minDist == distLeft || minDist == distRight) && isGrazeTop && activeTop)) {
        
        // 判定バッファ：横からかすった場合のみ、ばねの上にキャラを吸い寄せる（位置補正）
        Vector3 pos = player->GetPosition();
        float playerHalfHeight = (playerAABB.top - playerAABB.bottom) * 0.5f;
        float playerHalfWidth = (playerAABB.right - playerAABB.left) * 0.5f;
        
        if (minDist == distLeft && isGrazeTop) {
            // 左からかすった場合、少しだけ右(内側)に寄せる
            pos.x = blockAABB.left - playerHalfWidth + 0.1f;
        } else if (minDist == distRight && isGrazeTop) {
            // 右からかすった場合、少しだけ左(内側)に寄せる
            pos.x = blockAABB.right + playerHalfWidth - 0.1f;
        }
        
        pos.y = blockAABB.top + playerHalfHeight;
        player->SetPosition(pos);

        // 1. 速度の上書き
        vel.y = jumpVelocityVertical_;
        player->SetVelocity(vel);
        player->SetIsOnGround(false);

        // 2. ダッシュ状態の中断とダッシュ回数の全回復
        player->CancelDash();
        player->RefillDash();

        // 3. ヒットストップ（約1〜2フレーム：0.03秒）
        player->ApplyHitstop(0.03f);

        // バウンドアニメーション発動
        TriggerBounce(isFloating ? BounceDirection::kCenter : BounceDirection::kUp);

    } else if (minDist == distBottom && distBottom < threshold && activeBottom) {
        vel.y = -jumpVelocityVertical_;
        player->SetVelocity(vel);
        TriggerBounce(BounceDirection::kDown);
    } else if (minDist == distLeft && distLeft < threshold && activeLeft) {
        player->SetExternalVelocityX(-jumpVelocityHorizontal_);
        vel.y = 5.0f; // 少し上に浮かせることで接地判定を解除し、慣性がすぐに消されるのを防ぐ
        player->SetVelocity(vel);
        player->SetIsOnGround(false);
        TriggerBounce(BounceDirection::kLeft);
    } else if (minDist == distRight && distRight < threshold && activeRight) {
        player->SetExternalVelocityX(jumpVelocityHorizontal_);
        vel.y = 5.0f; // 少し上に浮かせる
        player->SetVelocity(vel);
        player->SetIsOnGround(false);
        TriggerBounce(BounceDirection::kRight);
    }
}

void JumpBlock::SetProperties(const nlohmann::json& properties) {
    if (properties.contains("jumpVelocityVertical")) {
        jumpVelocityVertical_ = properties["jumpVelocityVertical"].get<float>();
    }
    if (properties.contains("jumpVelocityHorizontal")) {
        jumpVelocityHorizontal_ = properties["jumpVelocityHorizontal"].get<float>();
    }
    // 古い形式の互換性維持
    if (properties.contains("jumpVelocity")) {
        jumpVelocityVertical_ = properties["jumpVelocity"].get<float>();
        jumpVelocityHorizontal_ = properties["jumpVelocity"].get<float>();
    }
}

#ifdef USE_IMGUI
void JumpBlock::DrawImGui() {
    if (ImGui::TreeNode("JumpBlock Spring Animation")) {
        ImGui::SliderFloat("Stiffness", &stiffness_, 50.0f, 600.0f);
        ImGui::SliderFloat("Damping", &damping_, 1.0f, 40.0f);
        ImGui::SliderFloat("Initial Squash", &initialSquash_, -0.8f, 0.0f);
        ImGui::SliderFloat("Initial Velocity", &initialVelocity_, 0.0f, 30.0f);
        ImGui::SliderFloat("Cross Scale Ratio", &crossScaleRatio_, 0.0f, 1.0f);
        ImGui::Text("Current Amount: %.3f", bounceAmount_);
        ImGui::Text("Current Velocity: %.3f", bounceVelocity_);
        ImGui::Text("Is Bouncing: %s", isBouncing_ ? "true" : "false");

        BounceDirection facing = DetermineFacingDirection();
        const char* dirStr = "Up";
        if (facing == BounceDirection::kRight) dirStr = "Right";
        else if (facing == BounceDirection::kLeft) dirStr = "Left";
        else if (facing == BounceDirection::kDown) dirStr = "Down";
        else if (facing == BounceDirection::kCenter) dirStr = "Center (Floating)";
        ImGui::Text("Facing Direction: %s", dirStr);
        ImGui::Text("Rotation Z: %.3f rad", baseRotation_.z);
        
        if (ImGui::Button("Test Bounce (Up)")) {
            TriggerBounce(BounceDirection::kUp);
        }
        ImGui::SameLine();
        if (ImGui::Button("Test Bounce (Down)")) {
            TriggerBounce(BounceDirection::kDown);
        }
        if (ImGui::Button("Test Bounce (Left)")) {
            TriggerBounce(BounceDirection::kLeft);
        }
        ImGui::SameLine();
        if (ImGui::Button("Test Bounce (Right)")) {
            TriggerBounce(BounceDirection::kRight);
        }
        ImGui::TreePop();
    }
}
#endif
