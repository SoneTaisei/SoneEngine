#include "Lvy.h"
#include "BlockFactory.h"
#include "Game2D/Player/Player2D.h"
#include "Game2D/MapChip2D.h"
#include "Core/TimeManager.h"
#include <cmath>
#include <algorithm>
#include <chrono>
#ifdef USE_IMGUI
#include "Editor/EditorManager.h"
#include <imgui.h>
#endif
#include "Editor/Replay/ReplayManager.h"

REGISTER_BLOCK_CLASS(Lvy);

namespace {
    uint32_t s_lvyGlobalFrame = 1;
    std::chrono::steady_clock::time_point s_lastLvyFrameTime{};

    // フレーム更新の判定
    void UpdateLvyFrameCounter() {
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - s_lastLvyFrameTime).count() >= 1) {
            s_lastLvyFrameTime = now;
            s_lvyGlobalFrame++;
            if (s_lvyGlobalFrame == 0) s_lvyGlobalFrame = 1;
        }
    }
}

void Lvy::Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) {
    baseWorldX_ = worldX;
    baseWorldY_ = worldY;
    width_ = width;
    height_ = height;
    thickness_ = 0.24f;

    SetGameObject(std::make_unique<GameObject>("Lvy"));
    auto* tc = GetGameObject()->AddComponent<TransformComponent>();
    auto* prc = GetGameObject()->AddComponent<PrimitiveRendererComponent>();

    prc->Initialize(device, boxPrimitive);
    // ツタらしい緑色のマテリアルカラー
    prc->SetColor({ 0.28f, 0.78f, 0.35f, 1.00f });
    prc->SetLightingType(1);

    tc->SetPosition({ worldX, worldY, 0.0f });
    tc->SetScale({ thickness_, height, 1.0f });
    tc->SetRotation({ 0.0f, 0.0f, 0.0f });

    SetupCollider();
}

Lvy::ChainInfo Lvy::GetChainInfo() const {
    ChainInfo info;
    info.rootBlock = const_cast<Lvy*>(this);
    info.tailBlock = const_cast<Lvy*>(this);
    info.totalLength = height_;
    info.totalSegments = 1;

    float chipSize = GetMap() ? GetMap()->GetChipSize() : height_;

    if (!GetMap()) {
        info.pivotPos = { baseWorldX_, baseWorldY_ + height_ * 0.5f, 0.0f };
        info.segments.push_back(const_cast<Lvy*>(this));
        return info;
    }

    // 上方向（chipY + 1）へ連続するLvyを探索して最上部（Root）を見つける
    int curY = GetChipY();
    while (true) {
        BaseBlock* above = GetMap()->GetBlock(GetChipX(), curY + 1);
        auto* lvyAbove = dynamic_cast<Lvy*>(above);
        if (!lvyAbove) break;
        curY++;
    }
    Lvy* root = dynamic_cast<Lvy*>(GetMap()->GetBlock(GetChipX(), curY));
    if (root) {
        info.rootBlock = root;
    }

    // 下方向（chipY - 1）へ連続するLvyを探索して最下部（Tail）を見つける
    int botY = GetChipY();
    while (true) {
        BaseBlock* below = GetMap()->GetBlock(GetChipX(), botY - 1);
        auto* lvyBelow = dynamic_cast<Lvy*>(below);
        if (!lvyBelow) break;
        botY--;
    }
    Lvy* tail = dynamic_cast<Lvy*>(GetMap()->GetBlock(GetChipX(), botY));
    if (tail) {
        info.tailBlock = tail;
    }

    // ピボット軸（固定回転軸）は最上部ブロックの上端中央（天井ブロックの底面）
    float rootCenterX = GetMap()->ChipToWorldX(info.rootBlock->GetChipX()) + chipSize * 0.5f;
    float rootTopY = GetMap()->ChipToWorldY(info.rootBlock->GetChipY()) + chipSize;
    info.pivotPos = { rootCenterX, rootTopY, 0.0f };

    // 最上部から最下部までの各ブロックを順番にリストアップ＆長さを集計
    float totalLen = 0.0f;
    int segs = 0;
    for (int y = curY; y >= botY; --y) {
        BaseBlock* block = GetMap()->GetBlock(GetChipX(), y);
        if (auto* lvy = dynamic_cast<Lvy*>(block)) {
            info.segments.push_back(lvy);
            totalLen += lvy->height_;
            segs++;
        }
    }

    info.totalLength = (std::max)(totalLen, 0.5f);
    info.totalSegments = segs;
    return info;
}

float Lvy::GetSegmentAngle(size_t index) const {
    if (index < segmentPhysics_.size()) {
        return segmentPhysics_[index].angle;
    }
    return 0.0f;
}

float Lvy::GetSegmentAngularVelocity(size_t index) const {
    if (index < segmentPhysics_.size()) {
        return segmentPhysics_[index].angularVelocity;
    }
    return 0.0f;
}

Lvy::VinePoint Lvy::GetPointAtDistance(const ChainInfo& chain, float distance) const {
    VinePoint pt;
    pt.position = chain.pivotPos;
    pt.velocity = { 0.0f, 0.0f, 0.0f };
    pt.segmentIndex = 0;

    size_t numSegs = chain.segments.size();
    if (numSegs == 0) return pt;

    float clampedDist = (std::clamp)(distance, 0.15f, chain.totalLength);

    float currentDist = 0.0f;
    Vector3 currentJoint = chain.pivotPos;
    Vector3 currentVel = { 0.0f, 0.0f, 0.0f };

    for (size_t i = 0; i < numSegs; ++i) {
        float segLen = chain.segments[i]->height_;
        float angle = GetSegmentAngle(i);
        float omega = GetSegmentAngularVelocity(i);

        // 目的の距離がこのセグメント内に収まっているか
        if (clampedDist <= currentDist + segLen || i == numSegs - 1) {
            float u = clampedDist - currentDist;
            u = (std::clamp)(u, 0.0f, segLen);

            pt.position = {
                currentJoint.x + std::sin(angle) * u,
                currentJoint.y - std::cos(angle) * u,
                currentJoint.z
            };
            pt.velocity = {
                currentVel.x + omega * u * std::cos(angle),
                currentVel.y + omega * u * std::sin(angle),
                0.0f
            };
            pt.segmentIndex = i;
            return pt;
        }

        // 次のセグメントへ進む
        currentJoint.x += std::sin(angle) * segLen;
        currentJoint.y -= std::cos(angle) * segLen;

        currentVel.x += omega * segLen * std::cos(angle);
        currentVel.y += omega * segLen * std::sin(angle);

        currentDist += segLen;
    }

    pt.position = currentJoint;
    pt.velocity = currentVel;
    pt.segmentIndex = numSegs - 1;
    return pt;
}

void Lvy::Update() {
    BaseBlock::Update();

    if (!isActive_) return;

    UpdateLvyFrameCounter();

    ChainInfo chain = GetChainInfo();
    Lvy* root = chain.rootBlock;
    if (!root) return;

    // 同一フレーム内の最初の1回でチェーン全体を一括更新
    if (root->lastUpdatedFrame_ != s_lvyGlobalFrame) {
        root->lastUpdatedFrame_ = s_lvyGlobalFrame;

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

        bool isPhysicsActive = isPlayingOrReplaying && !ReplayManager::GetInstance()->IsPaused();

        // 1. Rootがしなり物理シミュレーションを実行（各セグメントの角度・速度、プレイヤー拘束を更新）
        root->SimulatePhysics(chain, deltaTime, isPhysicsActive);

        // 2. 【ジョイント連鎖配置】各セグメントの端と端を完全に一致させて配置
        Vector3 currentJoint = chain.pivotPos;

        for (size_t i = 0; i < chain.segments.size(); ++i) {
            Lvy* seg = chain.segments[i];
            seg->lastUpdatedFrame_ = s_lvyGlobalFrame;

            float segLen = seg->height_;
            float angle = isPhysicsActive ? root->GetSegmentAngle(i) : 0.0f;

            Vector3 delta = {
                std::sin(angle) * segLen,
                -std::cos(angle) * segLen,
                0.0f
            };

            Vector3 segCenter = {
                currentJoint.x + delta.x * 0.5f,
                currentJoint.y + delta.y * 0.5f,
                currentJoint.z
            };

            if (seg->GetGameObject()) {
                if (auto* tc = seg->GetGameObject()->GetComponent<TransformComponent>()) {
                    tc->SetPosition(segCenter);
                    // 継ぎ目の完全密着オーバーラップ(+0.04f)
                    tc->SetScale({ seg->thickness_, segLen + 0.04f, 1.0f });
                    tc->SetRotation({ 0.0f, 0.0f, angle });
                }
            }

            currentJoint.x += delta.x;
            currentJoint.y += delta.y;
        }
    }
}

void Lvy::SimulatePhysics(const ChainInfo& chain, float deltaTime, bool isPhysicsActive) {
    size_t numSegs = chain.segments.size();
    if (numSegs == 0) return;

    if (segmentPhysics_.size() != numSegs) {
        segmentPhysics_.resize(numSegs, { 0.0f, 0.0f });
    }

    if (!isPhysicsActive) return;

    // クールダウン減算
    if (cooldownTimer_ > 0.0f) {
        cooldownTimer_ -= deltaTime;
    }

    // プレイヤー生存チェック
    if (attachedPlayer_) {
        if (attachedPlayer_->IsDead() || attachedPlayer_->IsGoal()) {
            attachedPlayer_->SetIsGrabbingVine(false);
            attachedPlayer_ = nullptr;
        }
    }

    // プレイヤーの操作入力
    float inputTorque = 0.0f;
    float dirX = 0.0f;
    if (attachedPlayer_) {
        const auto& input = attachedPlayer_->GetCurrentInput();
        dirX = input.moveX;
        if (std::abs(dirX) > 0.05f) {
            inputTorque = dirX * swingPower_;
        }

        // 上下移動（よじ登り・スライド降り）
        // moveY: -1.0f (下) 〜 1.0f (上)
        float moveY = input.moveY;
        if (std::abs(moveY) > 0.1f) {
            // 上キーでピボット側（上）へ登る、下キーで先端（下）へ降りる
            grabDistance_ -= moveY * climbSpeed_ * deltaTime;
            grabDistance_ = (std::clamp)(grabDistance_, 0.2f, chain.totalLength);
        }

        // ジャンプ入力による離脱（大ジャンプ射出）
        if (input.isJumpPressed || attachedPlayer_->GetState().jumpBufferTimer_ > 0.0f) {
            // 現在の掴まり位置における速度ベクトルを取得
            VinePoint pt = GetPointAtDistance(chain, grabDistance_);

            float releaseVelX = pt.velocity.x * jumpReleaseBonusX_;
            float releaseVelY = pt.velocity.y + jumpReleaseBonusY_;

            if (std::abs(releaseVelX) < 3.0f && std::abs(dirX) > 0.1f) {
                releaseVelX = (dirX > 0.0f ? 1.0f : -1.0f) * 7.0f;
            }

            attachedPlayer_->SetVelocity({ releaseVelX, releaseVelY, 0.0f });
            attachedPlayer_->SetIsGrabbingVine(false);
            attachedPlayer_->SetIsOnGround(false);
            attachedPlayer_->RefillDash();

            cooldownTimer_ = 0.4f;
            attachedPlayer_ = nullptr;
        }
    }

    // プレイヤーが現在つかまっているセグメントの取得
    size_t playerSegIndex = numSegs - 1;
    if (attachedPlayer_) {
        VinePoint pt = GetPointAtDistance(chain, grabDistance_);
        playerSegIndex = pt.segmentIndex;
    }

    // --- 【しなり物理シミュレーション (サブステップ積分)】 ---
    float damping = attachedPlayer_ ? dampingHolding_ : dampingEmpty_;
    float dt = (std::min)(deltaTime, 0.05f);
    const int subSteps = 5;
    float subDt = dt / static_cast<float>(subSteps);

    for (int step = 0; step < subSteps; ++step) {
        std::vector<float> angularAccels(numSegs, 0.0f);

        for (size_t i = 0; i < numSegs; ++i) {
            float theta = segmentPhysics_[i].angle;
            float omega = segmentPhysics_[i].angularVelocity;

            // 1. 重力復元トルク
            float tauGrav = -gravity_ * std::sin(theta);

            // 2. 上側セグメントとの曲げ剛性トルク
            float prevTheta = (i == 0) ? 0.0f : segmentPhysics_[i - 1].angle;
            float prevOmega = (i == 0) ? 0.0f : segmentPhysics_[i - 1].angularVelocity;
            float diffUp = theta - prevTheta;
            float tauBendUp = -bendStiffness_ * diffUp - bendDamping_ * (omega - prevOmega);

            // 3. 下側セグメントとの曲げ剛性トルク
            float tauBendDown = 0.0f;
            if (i + 1 < numSegs) {
                float nextTheta = segmentPhysics_[i + 1].angle;
                float nextOmega = segmentPhysics_[i + 1].angularVelocity;
                float diffDown = theta - nextTheta;
                tauBendDown = -bendStiffness_ * diffDown - bendDamping_ * (omega - nextOmega);
            }

            // 4. 空気抵抗減衰
            float tauDamp = -damping * omega;

            // 5. プレイヤーからの力（プレイヤーがいるセグメントに作用）
            float tauPlayer = 0.0f;
            if (attachedPlayer_ && i == playerSegIndex) {
                tauPlayer += inputTorque;
                // スイング遠心力によるしなり増幅
                tauPlayer += (theta > 0.0f ? 1.0f : -1.0f) * (omega * omega) * 0.18f;
                // 体重による牽引
                tauPlayer += 8.0f * (dirX != 0.0f ? dirX : std::sin(theta));
            }

            float massFactor = 1.0f + static_cast<float>(numSegs - 1 - i) * 0.35f;
            float totalTorque = tauGrav + tauBendUp + tauBendDown + tauDamp + tauPlayer;
            angularAccels[i] = totalTorque / massFactor;
        }

        for (size_t i = 0; i < numSegs; ++i) {
            segmentPhysics_[i].angularVelocity += angularAccels[i] * subDt;
            segmentPhysics_[i].angle += segmentPhysics_[i].angularVelocity * subDt;

            // 最大振れ角の制限
            if (segmentPhysics_[i].angle > maxAngle_) {
                segmentPhysics_[i].angle = maxAngle_;
                if (segmentPhysics_[i].angularVelocity > 0.0f) {
                    segmentPhysics_[i].angularVelocity = -segmentPhysics_[i].angularVelocity * 0.2f;
                }
            } else if (segmentPhysics_[i].angle < -maxAngle_) {
                segmentPhysics_[i].angle = -maxAngle_;
                if (segmentPhysics_[i].angularVelocity < 0.0f) {
                    segmentPhysics_[i].angularVelocity = -segmentPhysics_[i].angularVelocity * 0.2f;
                }
            }
        }
    }

    // プレイヤーが掴まっている場合、ツタ上の grabDistance_ の位置へ拘束
    if (attachedPlayer_) {
        VinePoint pt = GetPointAtDistance(chain, grabDistance_);
        attachedPlayer_->SetPosition(pt.position);
        attachedPlayer_->SetVelocity(pt.velocity);
        attachedPlayer_->SetIsOnGround(false);
    }
}

void Lvy::OnCollision(Player2D* player) {
    ChainInfo chain = GetChainInfo();
    if (chain.rootBlock) {
        chain.rootBlock->HandlePlayerGrab(player, chain);
    }
}

void Lvy::HandlePlayerGrab(Player2D* player, const ChainInfo& chain) {
    if (!player || attachedPlayer_ != nullptr || cooldownTimer_ > 0.0f) return;
    if (player->IsDead() || player->IsGoal()) return;

    // 各セグメントの折れ線（ポリライン）とプレイヤーとの最短距離および接触位置を判定
    Vector3 playerPos = player->GetPosition();
    Vector3 currentJoint = chain.pivotPos;
    float minDistanceSq = 999999.0f;
    float bestGrabDist = chain.totalLength;

    float currentDist = 0.0f;
    for (size_t i = 0; i < chain.segments.size(); ++i) {
        float segLen = chain.segments[i]->height_;
        float angle = GetSegmentAngle(i);

        Vector3 nextJoint = {
            currentJoint.x + std::sin(angle) * segLen,
            currentJoint.y - std::cos(angle) * segLen,
            currentJoint.z
        };

        Vector3 ab = { nextJoint.x - currentJoint.x, nextJoint.y - currentJoint.y, 0.0f };
        Vector3 ap = { playerPos.x - currentJoint.x, playerPos.y - currentJoint.y, 0.0f };
        float abLenSq = ab.x * ab.x + ab.y * ab.y;

        if (abLenSq > 0.0001f) {
            float t = (std::clamp)((ap.x * ab.x + ap.y * ab.y) / abLenSq, 0.0f, 1.0f);
            Vector3 nearest = { currentJoint.x + ab.x * t, currentJoint.y + ab.y * t, currentJoint.z };
            float distSq = (playerPos.x - nearest.x) * (playerPos.x - nearest.x) + (playerPos.y - nearest.y) * (playerPos.y - nearest.y);
            if (distSq < minDistanceSq) {
                minDistanceSq = distSq;
                bestGrabDist = currentDist + t * segLen; // 触れた瞬間のピボットからの距離
            }
        }
        currentJoint = nextJoint;
        currentDist += segLen;
    }

    float grabThreshold = 0.85f;
    if (minDistanceSq > grabThreshold * grabThreshold) {
        return; // まだツタに十分に触れていない
    }

    // つかまる！（触れた位置の座標・距離をそのまま保持）
    attachedPlayer_ = player;
    grabDistance_ = bestGrabDist;
    attachedPlayer_->SetIsGrabbingVine(true);
    attachedPlayer_->CancelDash();
    attachedPlayer_->RefillDash();

    // プレイヤーの突入速度をつかまったセグメントに伝達（初期しなり発生）
    VinePoint pt = GetPointAtDistance(chain, grabDistance_);
    float playerSpeedX = player->GetVelocity().x;
    if (pt.segmentIndex < segmentPhysics_.size()) {
        segmentPhysics_[pt.segmentIndex].angularVelocity += (playerSpeedX / chain.totalLength) * 1.0f;
    }
}

void Lvy::SetProperties(const nlohmann::json& properties) {
    if (properties.contains("gravity") && properties["gravity"].is_number()) {
        gravity_ = properties["gravity"].get<float>();
    }
    if (properties.contains("bendStiffness") && properties["bendStiffness"].is_number()) {
        bendStiffness_ = properties["bendStiffness"].get<float>();
    }
    if (properties.contains("bendDamping") && properties["bendDamping"].is_number()) {
        bendDamping_ = properties["bendDamping"].get<float>();
    }
    if (properties.contains("swingPower") && properties["swingPower"].is_number()) {
        swingPower_ = properties["swingPower"].get<float>();
    }
    if (properties.contains("jumpReleaseBonusY") && properties["jumpReleaseBonusY"].is_number()) {
        jumpReleaseBonusY_ = properties["jumpReleaseBonusY"].get<float>();
    }
    if (properties.contains("jumpReleaseBonusX") && properties["jumpReleaseBonusX"].is_number()) {
        jumpReleaseBonusX_ = properties["jumpReleaseBonusX"].get<float>();
    }
    if (properties.contains("climbSpeed") && properties["climbSpeed"].is_number()) {
        climbSpeed_ = properties["climbSpeed"].get<float>();
    }
}

void Lvy::Reset() {
    if (attachedPlayer_) {
        attachedPlayer_->SetIsGrabbingVine(false);
        attachedPlayer_ = nullptr;
    }
    for (auto& sp : segmentPhysics_) {
        sp.angle = 0.0f;
        sp.angularVelocity = 0.0f;
    }
    grabDistance_ = 0.0f;
    cooldownTimer_ = 0.0f;
    isActive_ = true;
    lastUpdatedFrame_ = 0;

    ChainInfo chain = GetChainInfo();
    Vector3 currentJoint = chain.pivotPos;

    for (size_t i = 0; i < chain.segments.size(); ++i) {
        Lvy* seg = chain.segments[i];
        float segLen = seg->height_;
        Vector3 segCenter = { currentJoint.x, currentJoint.y - segLen * 0.5f, currentJoint.z };

        if (seg->GetGameObject()) {
            if (auto* tc = seg->GetGameObject()->GetComponent<TransformComponent>()) {
                tc->SetPosition(segCenter);
                tc->SetScale({ thickness_, segLen + 0.04f, 1.0f });
                tc->SetRotation({ 0.0f, 0.0f, 0.0f });
            }
        }
        currentJoint.y -= segLen;
    }
}

AABB2D Lvy::GetAABB() const {
    ChainInfo chain = GetChainInfo();
    float sweepHalfWidth = chain.totalLength * std::sin(maxAngle_) + 0.6f;
    return {
        chain.pivotPos.x - sweepHalfWidth,
        chain.pivotPos.y + 0.5f,
        chain.pivotPos.x + sweepHalfWidth,
        chain.pivotPos.y - chain.totalLength - 0.5f
    };
}

#ifdef USE_IMGUI
void Lvy::DrawImGui() {
    ChainInfo chain = GetChainInfo();

    ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.4f, 1.0f), "[%s - マリオ風しなるツタ調整]", "Lvy (Ivy)");
    ImGui::Text("チェーン情報: %s (連結数: %d, 全長: %.2f)", 
        (this == chain.rootBlock) ? "親 (Root)" : "子 (Segment)", 
        chain.totalSegments, chain.totalLength);
    ImGui::Text("固定ピボット軸: (%.2f, %.2f)", chain.pivotPos.x, chain.pivotPos.y);
    ImGui::Separator();

    ImGui::DragFloat("重力定数 (Gravity)", &gravity_, 0.2f, 1.0f, 60.0f);
    ImGui::DragFloat("曲げ剛性・コシ (Bend Stiffness)", &bendStiffness_, 0.5f, 5.0f, 150.0f);
    ImGui::DragFloat("曲げ減衰 (Bend Damping)", &bendDamping_, 0.1f, 0.1f, 20.0f);
    ImGui::DragFloat("漕ぐ力 (Swing Power)", &swingPower_, 0.2f, 0.0f, 40.0f);
    ImGui::DragFloat("登り降り速度 (Climb Speed)", &climbSpeed_, 0.1f, 0.5f, 15.0f);
    ImGui::DragFloat("ジャンプ上昇力 (Jump Bonus Y)", &jumpReleaseBonusY_, 0.2f, 0.0f, 30.0f);
    ImGui::DragFloat("ジャンプ速度倍率 (Jump Bonus X)", &jumpReleaseBonusX_, 0.05f, 0.5f, 3.0f);

    ImGui::Separator();
    if (chain.rootBlock) {
        for (size_t i = 0; i < chain.rootBlock->segmentPhysics_.size(); ++i) {
            float deg = chain.rootBlock->segmentPhysics_[i].angle * 180.0f / 3.14159265f;
            ImGui::Text("節点[%zu] 角度: %.1f 度 (角速度: %.2f)", i, deg, chain.rootBlock->segmentPhysics_[i].angularVelocity);
        }
    }
    ImGui::Text("掴まり中: %s", (chain.rootBlock && chain.rootBlock->attachedPlayer_) ? "YES" : "NO");
    if (chain.rootBlock && chain.rootBlock->attachedPlayer_) {
        ImGui::Text("掴まり位置 (ピボットからの距離): %.2f / %.2f m", chain.rootBlock->grabDistance_, chain.totalLength);
    }

    if (ImGui::Button("先端を左へ押す (-8)")) {
        if (chain.rootBlock && !chain.rootBlock->segmentPhysics_.empty()) {
            chain.rootBlock->segmentPhysics_.back().angularVelocity -= 8.0f;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("先端を右へ押す (+8)")) {
        if (chain.rootBlock && !chain.rootBlock->segmentPhysics_.empty()) {
            chain.rootBlock->segmentPhysics_.back().angularVelocity += 8.0f;
        }
    }

    if (chain.rootBlock && chain.rootBlock->attachedPlayer_ && ImGui::Button("手を離す (Release)")) {
        chain.rootBlock->attachedPlayer_->SetIsGrabbingVine(false);
        chain.rootBlock->attachedPlayer_ = nullptr;
        chain.rootBlock->cooldownTimer_ = 0.5f;
    }

    if (ImGui::Button("状態リセット (Reset)")) {
        Reset();
    }
}
#endif
