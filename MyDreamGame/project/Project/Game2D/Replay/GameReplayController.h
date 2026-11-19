#pragma once
#include <vector>
#include <memory>

class Player2D;
class MapChip2D;
class GameCamera;
class BaseEffect;

class GameReplayController {
public:
    GameReplayController() = default;
    ~GameReplayController() = default;

    // リプレイ再生中の処理（フレーム更新・ループ判定・マップ/プレイヤー/カメラの同期）
    // 状態を再構築（開始・ループ・シーク）したフレームでは true を返す
    bool UpdatePlayback(Player2D* player, MapChip2D* map, GameCamera* camera,
                        const std::vector<std::unique_ptr<BaseEffect>>& effects,
                        int& previousScore);

    // プレイ中のリプレイ自動録画（開始判定のみをUpdateWithMapの前に呼ぶ）
    void EnsureRecordingStarted(Player2D* player, MapChip2D* map, GameCamera* camera, bool canRecord);
    void UpdateRecording(Player2D* player, MapChip2D* map, GameCamera* camera, bool canRecord);

    // 録画停止（エディタ停止時など）
    void StopRecording();

    // 状態リセット
    void Reset();

    // フラグ取得
    bool WasPlayingLastFrame() const { return wasPlayingLastFrame_; }

private:
    bool wasPlayingLastFrame_ = false;
};
