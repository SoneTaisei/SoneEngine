#pragma once
#include "Core/Utility/Vector3.h"
#include <memory>
#include <string>

class Object3D;

/// <summary>
/// お宝（鎖の終端の重り）の表示専用クラス
/// 物理は持たない。位置はプレイヤー鎖の末端ノードから毎フレーム与えられる
/// モデルパス・スケール・色・自転はここに閉じる（宝石モデルへの差し替えは ChainParams のパスを変えるだけ）
/// 当たり半径（Chain2D::EndWeight）とは独立なので、モデルを大きくしても物理は変わらない
/// </summary>
class Treasure2D {
public:
    void Initialize(const std::string& modelDir, const std::string& modelFile, float scale);

    /// <summary>表示スケール（物理半径とは独立）</summary>
    void SetVisualScale(float scale);

    /// <summary>回転中の合図（色を明るくする）</summary>
    void SetHighlight(bool highlight);

    /// <summary>自転を進める（宝石モデル用。球では見えない）</summary>
    void AddSelfRotation(float deltaAngle);

    /// <summary>位置＝末端ノード、向き＝最後の節の方向＋自転</summary>
    void UpdateTransform(const Vector3& pos, const Vector3& prevNodePos);

    void Draw();

    Object3D* GetObject() const { return obj_.get(); }        // ヒエラルキー用
    const Vector3& GetPosition() const { return position_; }   // ゴール判定・敗北判定・カメラ用（z=0）

private:
    std::unique_ptr<Object3D> obj_;
    Vector3 position_ = { 0.0f, 0.0f, 0.0f };
    float scale_ = 0.3f;
    float selfAngle_ = 0.0f;
    bool highlight_ = false;
};
