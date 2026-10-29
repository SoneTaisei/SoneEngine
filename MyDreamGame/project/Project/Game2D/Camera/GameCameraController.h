#pragma once
#include "Core/Utility/Vector3.h"

class GameCamera;
class Player2D;
class MapChip2D;

class GameCameraController {
public:
    GameCameraController() = default;
    ~GameCameraController() = default;

    // カメラのセットアップ（直交投影、部屋設定、追従設定、スナップ）
    void Setup(GameCamera* camera, Player2D* player, MapChip2D* map);

    // 通常プレイ中の更新（追従ターゲット設定、部屋境界、右スティック見渡し回転）
    void Update(float deltaTime, Player2D* player, MapChip2D* map, bool isClear, bool isReplaying);

    // リスポーン時のカメラスナップ・部屋再設定
    void OnRespawn(MapChip2D* map);

    // カメラスケール・オフセットの初期化
    void ResetView();

    // エディタ停止中の追従・スナップ更新
    void UpdateEditor(Player2D* player, MapChip2D* map);

    // カメラの取得
    GameCamera* GetCamera() const { return camera_; }

    // 初期カメラスケールの取得
    float GetInitialScale() const { return initialCameraScale_; }

private:
    GameCamera* camera_ = nullptr;
    float initialCameraScale_ = 1.0f;
    Vector3 cameraLookRotation_ = { 0.0f, 0.0f, 0.0f };
};
