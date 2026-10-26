#include "GameReplayController.h"
#include "Game2D/Player/Player2D.h"
#include "Game2D/MapChip2D.h"
#include "Graphics/Camera/GameCamera.h"
#include "Effect/BaseEffect.h"
#include "Editor/Replay/ReplayManager.h"
#include "GameObject/PrimitiveObject.h"

void GameReplayController::Reset() {
    wasPlayingLastFrame_ = false;
}

void GameReplayController::StopRecording() {
    if (ReplayManager::GetInstance()->IsRecording()) {
        ReplayManager::GetInstance()->StopRecord();
    }
}

void GameReplayController::UpdatePlayback(Player2D* player, MapChip2D* map, GameCamera* camera,
                                        const std::vector<std::unique_ptr<BaseEffect>>& effects,
                                        int& previousScore) {
    if (ReplayManager::GetInstance()->IsPlaying()) {
        // ループ判定を再生処理の一番最初で行う
        if (ReplayManager::GetInstance()->GetCurrentFrame() >= ReplayManager::GetInstance()->GetCurrentReplay().totalFrames) {
            if (ReplayManager::GetInstance()->IsLoopPlay()) {
                ReplayManager::GetInstance()->SetCurrentFrame(0); // これにより forceSnapNextFrame_ = true がセットされる
            } else {
                ReplayManager::GetInstance()->StopPlayback();
            }
        }

        if (ReplayManager::GetInstance()->IsPlaying()) {
            bool shouldRebuildState = !wasPlayingLastFrame_ || ReplayManager::GetInstance()->IsForceSnapNextFrame();
            if (shouldRebuildState) {
                auto& replayData = ReplayManager::GetInstance()->GetCurrentReplay();
                int curFrame = ReplayManager::GetInstance()->GetCurrentFrame();

                // 1. マップを初期状態（文字列）から復元
                if (!replayData.mapDataStr.empty() && map) {
                    map->LoadFromString(replayData.mapDataStr);
                }

                // 2. プレイヤー状態(速度含む)とスコアをリセット
                if (player) {
                    player->ResetState(replayData.playerInitPos);
                    player->ClearEffects();
                }
                for (auto& effect : effects) {
                    if (effect) effect->Reset();
                }

                // 3. 0フレーム目から現在フレームまで、記録された座標をたどってコインを回収
                if (player) {
                    for (int i = 0; i <= curFrame; ++i) {
                        player->SetPosition(replayData.frames[i].position);
                    }
                }
                if (map) {
                    map->SetRebuildEnabled(true);
                }

                // 4. コイン回収用に座標を動かしたので、シミュレーション再開用の正しい座標に戻す
                if (player) {
                    if (curFrame == 0) {
                        player->SetPosition(replayData.playerInitPos);
                    } else {
                        player->SetPosition(replayData.frames[curFrame - 1].position);
                    }
                    previousScore = player->GetScore();
                }
            }

            if (!wasPlayingLastFrame_) {
                // 再生開始時に初期位置へ自動ワープ
                if (player) {
                    player->SetPosition(ReplayManager::GetInstance()->GetCurrentReplay().playerInitPos);
                }
                if (camera) {
                    if (ReplayManager::GetInstance()->IsSnapEnabled()) {
                        camera->SetFollowTarget(nullptr);
                    } else {
                        if (player) camera->SetFollowTarget(&player->GetPosition());
                    }
                    camera->SetTranslation(ReplayManager::GetInstance()->GetCurrentReplay().cameraInitPos);
                }
                wasPlayingLastFrame_ = true;
            }

            Vector3 pos = player ? player->GetPosition() : Vector3{0.0f, 0.0f, 0.0f};
            Vector3 camPos = camera ? camera->GetTranslation() : Vector3{0.0f, 0.0f, 0.0f};
            ReplayManager::GetInstance()->UpdatePlayback(pos, camPos);
            if (player) {
                player->SetPosition(pos);
            }
            if (camera) {
                if (ReplayManager::GetInstance()->IsSnapEnabled()) {
                    camera->SetFollowTarget(nullptr);
                    camera->SetTranslation(camPos);
                } else {
                    if (player) camera->SetFollowTarget(&player->GetPosition());
                }
            }
        }
    } else {
        if (wasPlayingLastFrame_) {
            // リプレイが終了した（またはTAKEOVERで停止した）瞬間に、カメラの追従を復元する
            if (camera && player) {
                camera->SetFollowTarget(&player->GetPosition());
            }
        }
        wasPlayingLastFrame_ = false;
    }
}

void GameReplayController::UpdateRecording(Player2D* player, MapChip2D* map, GameCamera* camera, bool isCurrentlyPlaying) {
    if (isCurrentlyPlaying && !ReplayManager::GetInstance()->IsPlaying()) {
        if (player) {
            Vector3 camPos = camera ? camera->GetTranslation() : Vector3{0.0f, 0.0f, 0.0f};
            if (!ReplayManager::GetInstance()->IsRecording()) {
                std::string mapStr = map ? map->GetMapDataAsString() : "";
                ReplayManager::GetInstance()->StartRecord(player->GetPosition(), camPos, mapStr);
            }
            Vector4 pColor = player->GetPrimitiveObject() ? player->GetPrimitiveObject()->GetMaterial().color : Vector4(1.0f, 1.0f, 1.0f, 1.0f);
            Vector3 pScale = player->GetPrimitiveObject() ? player->GetPrimitiveObject()->GetScale() : Vector3(1.0f, 1.0f, 1.0f);
            Vector3 pRot = player->GetPrimitiveObject() ? player->GetPrimitiveObject()->GetRotation() : Vector3(0.0f, 0.0f, 0.0f);
            ReplayManager::GetInstance()->RecordFrame(player->GetPosition(), camPos, pColor, pScale, pRot);
        }
    } else {
        if (ReplayManager::GetInstance()->IsRecording()) {
            ReplayManager::GetInstance()->StopRecord();
        }
    }
}
