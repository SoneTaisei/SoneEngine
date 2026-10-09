#include "PlayerStates.h"
#include "../Player2D.h"
#include "../../MapChip2D.h"
#include "Core/TimeManager.h"
#include "Editor/Replay/ReplayManager.h"
#include <algorithm>
#include <cmath>

// ===================================================================
// PlayerNormalState (通常操作・移動状態)
// ===================================================================

void PlayerNormalState::Enter(Player2D* player) {
    player->state_.isDead_ = false;
    player->state_.isGoal_ = false;
    player->state_.isRespawning_ = false;
}

void PlayerNormalState::Update(Player2D* player, MapChip2D& map, float deltaTime) {
    auto& state_ = player->state_;
    const auto& params_ = player->params_;
    auto& visuals_ = player->visuals_;
    auto& physics_ = player->physics_;
    const auto& currentInput_ = player->currentInput_;

    // 壁ジャンプタイマーの更新
    if (state_.wallJumpTimer_ > 0.0f) {
        state_.wallJumpTimer_ -= deltaTime;
    }

    // 物理演算と移動入力処理
    physics_.Update(state_, params_, currentInput_, visuals_, deltaTime, player);

    // 現在のルームを特定する
    const auto& rooms = map.GetRooms();
    bool isInAnyRoom = false;
    for (int i = 0; i < static_cast<int>(rooms.size()); ++i) {
        if (state_.position_.x >= rooms[i].x && state_.position_.x <= rooms[i].x + rooms[i].width &&
            state_.position_.y >= rooms[i].y && state_.position_.y <= rooms[i].y + rooms[i].height) {
            state_.currentRoomIndex_ = i;
            isInAnyRoom = true;
            break;
        }
    }

    // 完全にルームから逸脱している場合は死亡する
    if (!rooms.empty() && !isInAnyRoom && !state_.isDead_) {
        player->Kill(true);
        return;
    }

    float deathY = -10.0f;
    if (state_.currentRoomIndex_ >= 0 && state_.currentRoomIndex_ < static_cast<int>(rooms.size())) {
        // ルームの下端から少し余裕をもたせた高さをデスマッチラインとする
        deathY = rooms[state_.currentRoomIndex_].y - 2.0f;
    }

    // 画面外落下時の死亡移行
    if (state_.position_.y < deathY) {
        player->Kill(true);
        return;
    }

    // 色の更新
    visuals_.SetColor((state_.isDashing_ || !state_.canDash_) ? params_.colorDashed_ : params_.colorNormal_);

    // 走りエフェクトの発生
    if (state_.isOnGround_ && std::abs(state_.velocity_.x) > 0.1f) {
        state_.runDustTimer_ += deltaTime;
        if (state_.runDustTimer_ >= params_.runDustInterval_) {
            state_.runDustTimer_ = 0.0f;
            float dirX = (state_.velocity_.x > 0.0f) ? 1.0f : -1.0f;
            visuals_.SpawnRunDust({ state_.position_.x - dirX * params_.halfWidth_, state_.position_.y - params_.halfHeight_, 0.0f }, dirX);
        }
    } else {
        state_.runDustTimer_ = 0.0f;
    }

    // バグ検知処理（リプレイ再生中は無効化）
    if (!ReplayManager::GetInstance()->IsPlaying()) {
        if (state_.position_.y < (deathY - 50.0f) || std::isnan(state_.position_.x) || std::isnan(state_.position_.y)) {
            player->Kill(true);
            return;
        }
    }
}

void PlayerNormalState::Exit(Player2D* player) {
    // 状態を抜ける時の後処理（必要に応じて）
}

// ===================================================================
// PlayerDeadState (死亡演出状態)
// ===================================================================

void PlayerDeadState::Enter(Player2D* player) {
    auto& state_ = player->state_;
    state_.isDead_ = true;
    state_.isRespawning_ = false;
    state_.deathTimer_ = 0.0f;

    if (isFallDeath_) {
        // 落下・逸脱死の場合：上に跳ねず、下方向への初速を与えて重力で落とす
        state_.velocity_.y = -5.0f;
        state_.velocity_.x *= 0.2f;
        state_.isDashing_ = false;
        TimeManager::GetInstance().SetTimeScale(1.0f);
    } else {
        // 後ろによろける演出のための速度設定
        state_.velocity_ = { state_.velocity_.x > 0.0f ? -2.5f : (state_.velocity_.x < 0.0f ? 2.5f : -2.5f), 4.0f, 0.0f };
        state_.isDashing_ = false;
        // スローモーション開始
        TimeManager::GetInstance().SetTimeScale(0.3f);
    }
}

void PlayerDeadState::Update(Player2D* player, MapChip2D& map, float deltaTime) {
    auto& state_ = player->state_;
    const auto& params_ = player->params_;
    auto& visuals_ = player->visuals_;

    state_.deathTimer_ += deltaTime;

    // ノックバック物理挙動
    state_.velocity_.y += params_.gravity_ * deltaTime;
    state_.position_.x += state_.velocity_.x * deltaTime;
    state_.position_.y += state_.velocity_.y * deltaTime;

    // ディゾルブ演出の進行
    float t = (std::min)(state_.deathTimer_ / params_.deathDuration_, 1.0f);
    visuals_.SetDissolveThreshold(t);

    if (state_.deathTimer_ >= params_.deathDuration_) {
        // リスポーン地点の決定
        Vector3 respawnPos = state_.startPosition_;
        const auto& rooms = map.GetRooms();
        if (state_.currentRoomIndex_ >= 0 && state_.currentRoomIndex_ < static_cast<int>(rooms.size())) {
            const auto& room = rooms[state_.currentRoomIndex_];
            bool foundRespawn = false;
            for (int y = 0; y < map.GetHeight(); ++y) {
                for (int x = 0; x < map.GetWidth(); ++x) {
                    if (map.GetChipType(x, y) == MapChip2D::ChipType::kRoomRespawn) {
                        float wx = map.ChipToWorldX(x) + map.GetChipSize() * 0.5f;
                        float wy = map.ChipToWorldY(y) + map.GetChipSize() * 0.5f;
                        if (wx >= room.x && wx <= room.x + room.width &&
                            wy >= room.y && wy <= room.y + room.height) {
                            respawnPos = { wx, wy, 0.0f };
                            foundRespawn = true;
                            break;
                        }
                    }
                }
                if (foundRespawn) break;
            }
        }

        // 指定地点に復活
        state_.position_ = respawnPos;
        state_.velocity_ = { 0.0f, 0.0f, 0.0f };
        state_.isDead_ = false;
        state_.deathTimer_ = 0.0f;
        state_.isDashing_ = false;
        state_.canDash_ = true;
        state_.wallJumpDirLockTimer_ = 0.0f;
        state_.lockedDirectionX_ = 0.0f;

        // 慣性をリセット
        state_.isOnMovingPlatform_ = false;
        state_.platformVelocity_ = { 0.0f, 0.0f, 0.0f };
        state_.recentPlatformVelocity_ = { 0.0f, 0.0f, 0.0f };
        state_.wallPlatformVelocity_ = { 0.0f, 0.0f, 0.0f };
        state_.platformInertiaTimer_ = 0.0f;
        state_.externalVelocityX_ = 0.0f;
        state_.isWallClinging_ = false;
        state_.isWallSliding_ = false;
        state_.climbingUpTimer_ = 0.0f;
        state_.climbLandingTimer_ = 0.0f;

        // ステージ内の動的オブジェクト（敵やギミック等）を初期位置・状態にリセット
        map.ResetBlocks();

        // リスポーンステートへ遷移
        player->ChangeState(std::make_unique<PlayerRespawnState>());
    }
}

void PlayerDeadState::Exit(Player2D* player) {
    player->visuals_.SetDissolveThreshold(0.0f);
    TimeManager::GetInstance().SetTimeScale(1.0f); // スローモーション解除
}

// ===================================================================
// PlayerRespawnState (リスポーン演出状態)
// ===================================================================

void PlayerRespawnState::Enter(Player2D* player) {
    auto& state_ = player->state_;
    state_.isRespawning_ = true;
    state_.respawnTimer_ = 0.0f;
    player->visuals_.SetRespawnVisual(state_.position_, player->params_, 0.0f);
}

void PlayerRespawnState::Update(Player2D* player, MapChip2D& map, float deltaTime) {
    auto& state_ = player->state_;
    const auto& params_ = player->params_;
    auto& visuals_ = player->visuals_;

    state_.respawnTimer_ += deltaTime;
    float t = (std::min)(state_.respawnTimer_ / params_.respawnDuration_, 1.0f);

    // EaseOutBackによる弾むようなポップアップ
    float c1 = 1.70158f;
    float c3 = c1 + 1.0f;
    float p = t - 1.0f;
    float scaleProgress = 1.0f + c3 * (p * p * p) + c1 * (p * p);
    if (scaleProgress < 0.0f) scaleProgress = 0.0f;

    visuals_.SetRespawnVisual(state_.position_, params_, scaleProgress);

    if (t >= 1.0f) {
        // 通常状態へ復帰
        player->ChangeState(std::make_unique<PlayerNormalState>());
    }
}

void PlayerRespawnState::Exit(Player2D* player) {
    player->state_.isRespawning_ = false;
    player->visuals_.SetRespawnVisual(player->state_.position_, player->params_, 1.0f);
}

// ===================================================================
// PlayerGoalState (ゴール演出状態)
// ===================================================================

void PlayerGoalState::Enter(Player2D* player) {
    auto& state_ = player->state_;
    state_.isGoal_ = true;
    state_.goalTimer_ = 0.0f;
    state_.velocity_ = { 0.0f, 0.0f, 0.0f };
    state_.isDashing_ = false;
    player->visuals_.SpawnConfetti(state_.position_);
}

void PlayerGoalState::Update(Player2D* player, MapChip2D& map, float deltaTime) {
    player->state_.goalTimer_ += deltaTime;
    player->state_.velocity_ = { 0.0f, 0.0f, 0.0f };
}

void PlayerGoalState::Exit(Player2D* player) {
    // ゴール状態終了時（必要に応じて）
}
