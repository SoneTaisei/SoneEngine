#pragma once
#include "Core/Utility/Vector3.h"
#include <memory>

class Object3D;

/// <summary>
/// お宝（鎖の終端の重り）の表示専用クラス
/// 物理は持たない。位置はプレイヤー鎖の末端ノードから毎フレーム与えられる
/// 仮モデルは sphere（金色）。正式な宝石モデルが来たら Initialize のパスを差し替えるだけ
/// </summary>
class Treasure2D {
public:
    void Initialize(float radius);

    /// <summary>表示半径（物理側の末端ノード半径と揃える）</summary>
    void SetRadius(float radius);

    /// <summary>位置＝末端ノード、向き＝最後の節の方向</summary>
    void UpdateTransform(const Vector3& pos, const Vector3& prevNodePos);

    void Draw();

    Object3D* GetObject() const { return obj_.get(); }        // ヒエラルキー用
    const Vector3& GetPosition() const { return position_; }   // ゴール判定・敗北判定・カメラ用（z=0）

private:
    std::unique_ptr<Object3D> obj_;
    Vector3 position_ = { 0.0f, 0.0f, 0.0f };
    float radius_ = 0.3f;
};
