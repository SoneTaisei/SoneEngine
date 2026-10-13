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
    player->ResetActionFlags();
}

void PlayerNormalState::Update(Player2D* player, MapChip2D& map, float deltaTime) {
    const auto &state_ = player->GetState();

    // 壁ジャンプタイマーの更新
    player->UpdateWallJumpTimer(deltaTime);

    // 物理演算と移動入力処理
    player->UpdatePhysics(deltaTime);

    // 現在のルームを特定する
    const auto& rooms = map.GetRooms();
    bool isInAnyRoom = false;
    for (int i = 0; i < static_cast<int>(rooms.size()); ++i) {
        if (state_.position_.x >= rooms[i].x && state_.position_.x <= rooms[i].x + rooms[i].width &&
            state_.position_.y >= rooms[i].y && state_.position_.y <= rooms[i].y + rooms[i].height) {
            player->SetCurrentRoomIndex(i);
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
    player->UpdateVisualColor();

    // 走りエフェクトの発生
    player->UpdateRunDust(deltaTime);

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
    player->StartDeathSequence(isFallDeath_);
}

void PlayerDeadState::Update(Player2D* player, MapChip2D& map, float deltaTime) {
    const auto& state_ = player->GetState();
    const auto& params_ = player->GetParams();
    auto& visuals_ = player->GetVisuals();

    // ノックバック物理挙動と死亡タイマー加算
    player->UpdateDeathKnockback(deltaTime);

    // ディゾルブ演出の進行
    float t = (std::min)(player->GetDeathTimer() / params_.deathDuration_, 1.0f);
    visuals_.SetDissolveThreshold(t);

    if (player->GetDeathTimer() >= params_.deathDuration_) {
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

        // ステージ内の動的オブジェクト（敵やギミック等）を初期位置・状態にリセット
        map.ResetBlocks();

        // 指定地点へ安全にリスポーン
        player->RespawnAt(respawnPos);
    }
}

void PlayerDeadState::Exit(Player2D* player) {
    player->GetVisuals().SetDissolveThreshold(0.0f);
    TimeManager::GetInstance().SetTimeScale(1.0f); // スローモーション解除
}

// ===================================================================
// PlayerRespawnState (リスポーン演出状態)
// ===================================================================

void PlayerRespawnState::Enter(Player2D* player) {
    player->StartRespawnSequence();
}

void PlayerRespawnState::Update(Player2D* player, MapChip2D& map, float deltaTime) {
    const auto& state_ = player->GetState();
    const auto& params_ = player->GetParams();
    auto& visuals_ = player->GetVisuals();

    player->UpdateRespawnTimer(deltaTime);
    float t = (std::min)(player->GetRespawnTimer() / params_.respawnDuration_, 1.0f);

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
    player->FinishRespawn();
}

// ===================================================================
// PlayerGoalState (ゴール演出状態)
// ===================================================================

void PlayerGoalState::Enter(Player2D* player) {
    player->StartGoalSequence();
}

void PlayerGoalState::Update(Player2D* player, MapChip2D& map, float deltaTime) {
    player->UpdateGoalTimer(deltaTime);
}

void PlayerGoalState::Exit(Player2D* player) {
    // ゴール状態終了時（必要に応じて）
}
