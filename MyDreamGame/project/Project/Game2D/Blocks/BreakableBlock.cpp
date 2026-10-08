#include "BreakableBlock.h"
#include "BlockFactory.h"
#include "Game2D/Player/Player2D.h"
#include "Game2D/MapChip2D.h"
#include "Graphics/GameCamera.h"
#include "Core/TimeManager.h"
#include <random>
#include <cmath>
#include <algorithm>
#include <queue>
#include <set>
#ifdef USE_IMGUI
#include <imgui.h>
#endif

// BlockFactoryへの自動登録マクロ（プロジェクト起動時に登録されます）
REGISTER_BLOCK_CLASS(BreakableBlock);

void BreakableBlock::Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) {
    basePosition_ = { worldX, worldY, 0.0f };
    baseScale_ = { width, height, 1.0f };
    isBroken_ = false;
    respawnTimer_ = 0.0f;

    // 本体 GameObject の生成
    gameObject_ = std::make_unique<GameObject>("BreakableBlock");
    auto* tc = gameObject_->AddComponent<TransformComponent>();
    auto* prc = gameObject_->AddComponent<PrimitiveRendererComponent>();

    prc->Initialize(device, boxPrimitive);
    prc->GetMaterial().color = blockColor_;
    tc->SetScale(baseScale_);
    tc->SetPosition(basePosition_);
    prc->GetMaterial().lightingType = 1;

    // コライダーの初期設定（通常ブロックと同様にSolid）
    SetupCollider();

    // 破片パーティクル用 GameObject の生成 (6個)
    debrisList_.clear();
    const int kDebrisCount = 6;
    for (int i = 0; i < kDebrisCount; ++i) {
        Debris d;
        d.gameObject = std::make_unique<GameObject>("BreakableDebris_" + std::to_string(i));
        auto* dtc = d.gameObject->AddComponent<TransformComponent>();
        auto* dprc = d.gameObject->AddComponent<PrimitiveRendererComponent>();

        dprc->Initialize(device, boxPrimitive);
        dprc->GetMaterial().color = blockColor_;
        dprc->GetMaterial().lightingType = 1;

        d.initialScale = { width * 0.35f, height * 0.35f, 0.35f };
        dtc->SetScale(d.initialScale);
        dtc->SetPosition(basePosition_);
        d.active = false;
        debrisList_.push_back(std::move(d));
    }
}

void BreakableBlock::Update() {
    float deltaTime = TimeManager::GetInstance().GetDeltaTime();

    if (!isBroken_) {
        // 通常時：普通のブロックとしてGameObjectを更新
        BaseBlock::Update();
    } else {
        // 破壊状態時：自動復活の設定があればタイマーを加算
        if (respawnTime_ > 0.0f) {
            respawnTimer_ += deltaTime;
            if (respawnTimer_ >= respawnTime_) {
                Respawn();
            }
        }
    }

    // 破片パーティクルの更新
    for (auto& debris : debrisList_) {
        if (!debris.active || !debris.gameObject) continue;

        debris.timer += deltaTime;
        if (debris.timer >= debris.lifetime) {
            debris.active = false;
            continue;
        }

        // 重力適用
        debris.velocity.y -= 25.0f * deltaTime;

        // 位置と回転の更新
        if (auto* tc = debris.gameObject->GetComponent<TransformComponent>()) {
            Vector3 pos = tc->GetPosition();
            pos.x += debris.velocity.x * deltaTime;
            pos.y += debris.velocity.y * deltaTime;
            pos.z += debris.velocity.z * deltaTime;
            tc->SetPosition(pos);

            Vector3 rot = tc->GetRotation();
            rot.x += debris.rotationVelocity.x * deltaTime;
            rot.y += debris.rotationVelocity.y * deltaTime;
            rot.z += debris.rotationVelocity.z * deltaTime;
            tc->SetRotation(rot);

            // 時間経過で縮小
            float progress = debris.timer / debris.lifetime;
            float scaleFactor = (std::max)(0.05f, 1.0f - progress * 0.5f);
            tc->SetScale({
                debris.initialScale.x * scaleFactor,
                debris.initialScale.y * scaleFactor,
                debris.initialScale.z * scaleFactor
            });
        }

        // フェードアウト
        if (auto* prc = debris.gameObject->GetComponent<PrimitiveRendererComponent>()) {
            float progress = debris.timer / debris.lifetime;
            prc->GetMaterial().color.w = (std::max)(0.0f, 1.0f - progress);
        }

        debris.gameObject->Update();
    }
}

void BreakableBlock::Draw() {
    if (!isBroken_) {
        // 通常時は本体を描画
        BaseBlock::Draw();
    }

    // 破壊時の破片を描画
    for (auto& debris : debrisList_) {
        if (debris.active && debris.gameObject) {
            debris.gameObject->Draw();
        }
    }
}

bool BreakableBlock::ShouldBreakFromContact(Player2D* player) const {
    if (!player || !player->IsDashing()) {
        return false;
    }

    // 方向制限が無効な場合はダッシュ中であれば無条件で破壊
    if (!requireDirectionalDash_) {
        return true;
    }

    AABB2D blockAABB = GetAABB();
    AABB2D playerAABB = player->GetAABB();

    // プレイヤーのダッシュ速度ベクトルを取得
    Vector3 dashVel = player->GetDashVelocity();
    float dashSpeedSq = dashVel.x * dashVel.x + dashVel.y * dashVel.y;
    if (dashSpeedSq < 0.001f) {
        dashVel = player->GetVelocity();
    }

    float blockCenterY = (blockAABB.top + blockAABB.bottom) * 0.5f;
    float blockCenterX = (blockAABB.left + blockAABB.right) * 0.5f;
    float playerCenterY = (playerAABB.top + playerAABB.bottom) * 0.5f;
    float playerCenterX = (playerAABB.left + playerAABB.right) * 0.5f;

    // 各軸での重なり量（正の値）
    float overlapX = (std::min)(playerAABB.right, blockAABB.right) - (std::max)(playerAABB.left, blockAABB.left);
    float overlapY = (std::min)(playerAABB.top, blockAABB.top) - (std::max)(playerAABB.bottom, blockAABB.bottom);

    // 水平方向の重なりが大きい場合は、上下からの接触
    if (overlapX > overlapY) {
        if (playerCenterY > blockCenterY) {
            // 上面からの接触：下向きのダッシュ（下、左下、右下）のみ壊れる
            // （横ダッシュや上ダッシュでは足元のブロックは壊れない）
            if (dashVel.y < -0.1f) {
                return true;
            }
        } else {
            // 下面からの接触：上向きのダッシュ（上、左上、右上）のみ壊れる
            if (dashVel.y > 0.1f) {
                return true;
            }
        }
    } else {
        // 垂直方向の重なりが大きい場合は、左右からの接触
        if (playerCenterX < blockCenterX) {
            // 左面からの接触：右向きのダッシュ（右、右上、右下）のみ壊れる
            if (dashVel.x > 0.1f) {
                return true;
            }
        } else {
            // 右面からの接触：左向きのダッシュ（左、左上、左下）のみ壊れる
            if (dashVel.x < -0.1f) {
                return true;
            }
        }
    }

    // 角（コーナー）付近のフォールバック
    // overlapX と overlapY がどちらも非常に小さい（<= 0.05f）場合、
    // ダッシュがブロックの中心に向かっているか（内積判定）で判定
    if (overlapX <= 0.05f && overlapY <= 0.05f) {
        float toBlockX = blockCenterX - playerCenterX;
        float toBlockY = blockCenterY - playerCenterY;
        if (toBlockX * dashVel.x + toBlockY * dashVel.y > 0.5f) {
            return true;
        }
    }

    return false;
}

void BreakableBlock::OnCollision(Player2D* player) {
    if (isBroken_ || !player) return;

    // 接触面とダッシュ方向が一致した場合のみ破壊
    if (ShouldBreakFromContact(player)) {
        Break(player);
    }
}

void BreakableBlock::OnPlayerStand() {
    // 通常の歩行ジャンプ等で乗った場合は普通の足場として乗れる
}

void BreakableBlock::OnPlayerTouch() {
    // 横から触れた場合も同様に通常ブロックとして機能
}

void BreakableBlock::Break(Player2D* player, bool triggerChain) {
    if (isBroken_) return;

    // プレイヤーのダッシュ方向を取得（連鎖ブロックの破片にも伝播）
    Vector3 dashDir = { 1.0f, 0.0f, 0.0f };
    if (player) {
        Vector3 vel = player->GetDashVelocity();
        float len = std::sqrt(vel.x * vel.x + vel.y * vel.y);
        if (len > 0.001f) {
            dashDir = { vel.x / len, vel.y / len, 0.0f };
        }
    }

    // 連結しているブロックを探索
    std::vector<BreakableBlock*> connectedBlocks;
    if (triggerChain && breakConnected_ && map_) {
        std::queue<std::pair<int, int>> queue;
        std::set<std::pair<int, int>> visited;

        queue.push({ chipX_, chipY_ });
        visited.insert({ chipX_, chipY_ });

        const int dx[4] = { 1, -1, 0, 0 };
        const int dy[4] = { 0, 0, 1, -1 };

        while (!queue.empty()) {
            auto [cx, cy] = queue.front();
            queue.pop();

            for (int i = 0; i < 4; ++i) {
                int nx = cx + dx[i];
                int ny = cy + dy[i];

                if (visited.count({ nx, ny })) continue;
                visited.insert({ nx, ny });

                BaseBlock* neighbor = map_->GetBlock(nx, ny);
                if (neighbor) {
                    if (auto* bBlock = dynamic_cast<BreakableBlock*>(neighbor)) {
                        if (!bBlock->IsBroken()) {
                            queue.push({ nx, ny });
                            connectedBlocks.push_back(bBlock);
                        }
                    }
                }
            }
        }
    }

    // 自身の破壊処理を実行
    BreakInternal(player, dashDir);

    // 連結しているブロックを一括破壊
    for (auto* block : connectedBlocks) {
        if (!block->IsBroken()) {
            block->BreakInternal(nullptr, dashDir);
        }
    }
}

void BreakableBlock::BreakInternal(Player2D* player, const Vector3& inheritedDashDir) {
    if (isBroken_) return;
    isBroken_ = true;
    respawnTimer_ = 0.0f;

    // コライダーを無効化（すり抜け可能にする）
    if (gameObject_) {
        if (auto* cc = gameObject_->GetComponent<ColliderComponent>()) {
            cc->SetLayerMask(0);
            cc->SetIsSolid(false);
        }
        if (auto* prc = gameObject_->GetComponent<PrimitiveRendererComponent>()) {
            blockColor_ = prc->GetMaterial().color;
        }
    }

    // ダッシュ方向の決定（playerがいれば取得、いなければ継承された方向）
    Vector3 dashDir = inheritedDashDir;
    if (player) {
        Vector3 vel = player->GetDashVelocity();
        float len = std::sqrt(vel.x * vel.x + vel.y * vel.y);
        if (len > 0.001f) {
            dashDir = { vel.x / len, vel.y / len, 0.0f };
        }
    }

    // 破片を四方に射出
    std::mt19937 rng(1337 + chipX_ * 100 + chipY_);
    std::uniform_real_distribution<float> distAngle(-3.14159f, 3.14159f);
    std::uniform_real_distribution<float> distSpeed(4.0f, 9.0f);
    std::uniform_real_distribution<float> distRot(-12.0f, 12.0f);
    std::uniform_real_distribution<float> distOffset(-0.2f, 0.2f);

    for (size_t i = 0; i < debrisList_.size(); ++i) {
        auto& d = debrisList_[i];
        d.active = true;
        d.timer = 0.0f;
        d.lifetime = 0.45f + (i % 3) * 0.1f;

        Vector3 spawnPos = {
            basePosition_.x + distOffset(rng) * baseScale_.x,
            basePosition_.y + distOffset(rng) * baseScale_.y,
            basePosition_.z
        };

        if (auto* tc = d.gameObject->GetComponent<TransformComponent>()) {
            tc->SetPosition(spawnPos);
            tc->SetScale(d.initialScale);
            tc->SetRotation({ 0.0f, 0.0f, 0.0f });
        }
        if (auto* prc = d.gameObject->GetComponent<PrimitiveRendererComponent>()) {
            prc->GetMaterial().color = blockColor_;
            prc->GetMaterial().color.w = 1.0f;
        }

        float speed = distSpeed(rng);
        float angle = distAngle(rng);
        d.velocity = {
            dashDir.x * 5.0f + std::cos(angle) * speed,
            dashDir.y * 5.0f + std::sin(angle) * speed + 3.0f,
            0.0f
        };
        d.rotationVelocity = { distRot(rng), distRot(rng), distRot(rng) };
    }

    // プレイヤー演出とフィーリング
    if (player) {
        // ヒットストップ
        if (hitstopDuration_ > 0.0f) {
            player->ApplyHitstop(hitstopDuration_);
        }

        // カメラシェイク
        if (player->GetCamera() && cameraShakePower_ > 0.0f) {
            player->GetCamera()->Shake(cameraShakePower_, cameraShakeDuration_);
        }

        // ダッシュ速度を復元してブロックを突き抜ける爽快感を実現
        player->SetVelocity(player->GetDashVelocity());
        player->SetIsOnGround(false);
    }
}

void BreakableBlock::Respawn() {
    isBroken_ = false;
    respawnTimer_ = 0.0f;

    // コライダーの復元
    if (gameObject_) {
        if (auto* cc = gameObject_->GetComponent<ColliderComponent>()) {
            cc->SetLayerMask(kLayerBlock);
            cc->SetIsSolid(true);
        }
        if (auto* tc = gameObject_->GetComponent<TransformComponent>()) {
            tc->SetPosition(basePosition_);
            tc->SetScale(baseScale_);
        }
        if (auto* prc = gameObject_->GetComponent<PrimitiveRendererComponent>()) {
            prc->GetMaterial().color = blockColor_;
            prc->GetMaterial().color.w = 1.0f;
        }
    }

    // 破片を非アクティブ化
    for (auto& debris : debrisList_) {
        debris.active = false;
    }
}

void BreakableBlock::SetProperties(const nlohmann::json& properties) {
    if (properties.contains("breakConnected") && properties["breakConnected"].is_boolean()) {
        breakConnected_ = properties["breakConnected"].get<bool>();
    }
    if (properties.contains("requireDirectionalDash") && properties["requireDirectionalDash"].is_boolean()) {
        requireDirectionalDash_ = properties["requireDirectionalDash"].get<bool>();
    }
    if (properties.contains("respawnTime") && properties["respawnTime"].is_number()) {
        respawnTime_ = properties["respawnTime"].get<float>();
    }
    if (properties.contains("hitstopDuration") && properties["hitstopDuration"].is_number()) {
        hitstopDuration_ = properties["hitstopDuration"].get<float>();
    }
    if (properties.contains("cameraShakePower") && properties["cameraShakePower"].is_number()) {
        cameraShakePower_ = properties["cameraShakePower"].get<float>();
    }
    if (properties.contains("cameraShakeDuration") && properties["cameraShakeDuration"].is_number()) {
        cameraShakeDuration_ = properties["cameraShakeDuration"].get<float>();
    }
    if (properties.contains("color") && properties["color"].is_array() && properties["color"].size() >= 3) {
        blockColor_.x = properties["color"][0].get<float>();
        blockColor_.y = properties["color"][1].get<float>();
        blockColor_.z = properties["color"][2].get<float>();
        if (properties["color"].size() >= 4) {
            blockColor_.w = properties["color"][3].get<float>();
        }
        if (gameObject_) {
            if (auto* prc = gameObject_->GetComponent<PrimitiveRendererComponent>()) {
                prc->GetMaterial().color = blockColor_;
            }
        }
    }
}

void BreakableBlock::Reset() {
    Respawn();
}

#ifdef USE_IMGUI
void BreakableBlock::DrawImGui() {
    ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.3f, 1.0f), "[BreakableBlock パラメータ調整]");
    ImGui::Text("状態: %s", isBroken_ ? "破壊中" : "通常");
    
    if (isBroken_) {
        if (respawnTime_ > 0.0f) {
            ImGui::ProgressBar(respawnTimer_ / respawnTime_, ImVec2(0.0f, 0.0f), "復活中");
        } else {
            ImGui::TextDisabled("自動復活なし (リトライ時に復活)");
        }
    }

    if (ImGui::Button("テスト破壊 (Break)")) {
        Break(nullptr);
    }
    ImGui::SameLine();
    if (ImGui::Button("復活 (Respawn)")) {
        Respawn();
    }

    ImGui::Separator();
    ImGui::Checkbox("連結ブロックを一括破壊 (Break Connected)", &breakConnected_);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("有効時：上下左右に隣接している壊せるブロックも連鎖して同時に破壊されます。");
    }
    ImGui::Checkbox("接触面に応じた方向ダッシュのみで破壊", &requireDirectionalDash_);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("有効時：上面接触時は下ダッシュ、横面接触時は横ダッシュでのみ破壊されます。\n無効時：ダッシュ中であればどの方向からでも破壊されます。");
    }
    ImGui::DragFloat("自動復活時間(秒, 0で無効)", &respawnTime_, 0.1f, 0.0f, 30.0f);
    ImGui::DragFloat("ヒットストップ(秒)", &hitstopDuration_, 0.01f, 0.0f, 0.5f);
    ImGui::DragFloat("画面揺れ強度", &cameraShakePower_, 0.05f, 0.0f, 2.0f);
    ImGui::DragFloat("画面揺れ時間(秒)", &cameraShakeDuration_, 0.02f, 0.0f, 1.0f);

    float col[4] = { blockColor_.x, blockColor_.y, blockColor_.z, blockColor_.w };
    if (ImGui::ColorEdit4("ブロック色", col)) {
        blockColor_ = { col[0], col[1], col[2], col[3] };
        if (gameObject_) {
            if (auto* prc = gameObject_->GetComponent<PrimitiveRendererComponent>()) {
                prc->GetMaterial().color = blockColor_;
            }
        }
    }
}
#endif
