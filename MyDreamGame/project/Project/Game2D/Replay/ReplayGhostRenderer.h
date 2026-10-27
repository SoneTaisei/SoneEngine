#pragma once
#include "Core/Utility/Matrix4x4.h"

class Player2D;

class ReplayGhostRenderer {
public:
    ReplayGhostRenderer() = default;
    ~ReplayGhostRenderer() = default;

    // 3D空間内のゴースト描画（リプレイ残像＆AIゴースト）
    void Draw3D(const Player2D* player);

    // エディタ画面上の2D軌跡オーバーレイ描画（ImGui ImDrawList使用）
    void DrawEditorOverlay(const Player2D* player, const Matrix4x4& viewProjectionMatrix);
};
