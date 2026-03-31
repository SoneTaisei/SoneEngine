#include "GameCamera.h"

void GameCamera::Initialize(int kClientWidth, int kClientHeight) {
    Camera::Initialize(kClientWidth, kClientHeight);
    // 初期位置の設定（例えばプレイヤーの少し後ろ）
    transform_.translate = { 0.0f, 0.0f, -10.0f };
    transform_.rotate = { 0.0f, 0.0f, 0.0f };
    UpdateMatrix();
}

void GameCamera::Update() {
    // 将来ここに「プレイヤーの座標を取得して、translate_ に代入する」処理を書く

    // 行列更新
    UpdateMatrix();
}
