#pragma once
#include "Camera.h"

// 普通のカメラ（将来プレイヤー追従などを入れる場所）
class GameCamera : public Camera {
public:
    void Initialize(int kClientWidth, int kClientHeight) override;
    void Update(); // 特に操作はないが、追従処理などをここに書く
};