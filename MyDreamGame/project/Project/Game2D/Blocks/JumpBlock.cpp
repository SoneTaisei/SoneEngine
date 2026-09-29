#include "JumpBlock.h"
#include "../Player/Player2D.h"
#include "../MapChip2D.h"
#include "Core/TimeManager.h"
#include <cmath>
#include <algorithm>
#ifdef USE_IMGUI
#include "Editor/EditorManager.h"
#include <imgui.h>
#endif
#include "Editor/Replay/ReplayManager.h"

void JumpBlock::Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) {
    gameObject_ = std::make_unique<GameObject>("JumpBlock");
    auto* tc = gameObject_->AddComponent<TransformComponent>();
    auto* prc = gameObject_->AddComponent<PrimitiveRendererComponent>();

    prc->Initialize(device, boxPrimitive);
    
    // ジャンプ台の色：オレンジ色
    prc->GetMaterial().color = { 1.0f, 0.5f, 0.0f, 1.0f };
    
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
        return;
    }

    // スカッシュ＆ストレッチ変形率（主軸と垂直副軸）
    float mainFactor = 1.0f + bounceAmount_;
    mainFactor = (std::max)(0.2f, mainFactor);
    float crossFactor = 1.0f - bounceAmount_ * crossScaleRatio_;
    crossFactor = (std::max)(0.2f, crossFactor);

    Vector3 currentScale = baseScale_;
    Vector3 currentPos = basePosition_;

    switch (bounceDir_) {
    case BounceDirection::kUp: {
        // 主軸：Y、副軸：X
        currentScale.y = baseScale_.y * mainFactor;
        currentScale.x = baseScale_.x * crossFactor;
        // 底面を固定（底面 Y = basePos.y - baseScale.y * 0.5f）
        float bottomY = basePosition_.y - baseScale_.y * 0.5f;
        currentPos.y = bottomY + currentScale.y * 0.5f;
        break;
    }
    case BounceDirection::kDown: {
        // 主軸：Y、副軸：X
        currentScale.y = baseScale_.y * mainFactor;
        currentScale.x = baseScale_.x * crossFactor;
        // 上面を固定（上面 Y = basePos.y + baseScale.y * 0.5f）
        float topY = basePosition_.y + baseScale_.y * 0.5f;
        currentPos.y = topY - currentScale.y * 0.5f;
        break;
    }
    case BounceDirection::kLeft: {
        // 主軸：X、副軸：Y
        currentScale.x = baseScale_.x * mainFactor;
        currentScale.y = baseScale_.y * crossFactor;
        // 右面を固定（右面 X = basePos.x + baseScale.x * 0.5f）
        float rightX = basePosition_.x + baseScale_.x * 0.5f;
        currentPos.x = rightX - currentScale.x * 0.5f;
        break;
    }
    case BounceDirection::kRight: {
        // 主軸：X、副軸：Y
        currentScale.x = baseScale_.x * mainFactor;
        currentScale.y = baseScale_.y * crossFactor;
        // 左面を固定（左面 X = basePos.x - baseScale.x * 0.5f）
        float leftX = basePosition_.x - baseScale_.x * 0.5f;
        currentPos.x = leftX + currentScale.x * 0.5f;
        break;
    }
    case BounceDirection::kCenter:
    default: {
        // 浮遊：中心基準で伸縮
        currentScale.y = baseScale_.y * mainFactor;
        currentScale.x = baseScale_.x * crossFactor;
        break;
    }
    }

    tc->SetPosition(currentPos);
    tc->SetScale(currentScale);
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
    
    // 周囲のブロック状況を取得
    bool hasRight  = map_->GetBlock(chipX_ + 1, chipY_) != nullptr;
    bool hasLeft   = map_->GetBlock(chipX_ - 1, chipY_) != nullptr;
    bool hasTop    = map_->GetBlock(chipX_, chipY_ + 1) != nullptr;
    bool hasBottom = map_->GetBlock(chipX_, chipY_ - 1) != nullptr;

    bool isFloating = (!hasRight && !hasLeft && !hasTop && !hasBottom);

    // 接地面（ブロックがくっついている面）を優先度順に判定し、ばねの方向を一つに絞る
    bool activeTop = false;
    bool activeBottom = false;
    bool activeLeft = false;
    bool activeRight = false;

    if (hasBottom) {
        activeTop = true; // 下にブロックがあるなら上面で跳ねる
    } else if (hasLeft) {
        activeRight = true; // 左にブロックがあるなら右面で跳ねる
    } else if (hasRight) {
        activeLeft = true; // 右にブロックがあるなら左面で跳ねる
    } else if (hasTop) {
        activeBottom = true; // 上にブロックがあるなら下面で跳ねる
    } else {
        activeTop = true; // 完全に浮いている場合はデフォルトで上面で跳ねる
    }

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

        // 2. ダッシュ回数の全回復
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
        
        if (ImGui::Button("Test Bounce (Up)")) {
            TriggerBounce(BounceDirection::kUp);
        }
        ImGui::SameLine();
        if (ImGui::Button("Test Bounce (Right)")) {
            TriggerBounce(BounceDirection::kRight);
        }
        ImGui::TreePop();
    }
}
#endif
