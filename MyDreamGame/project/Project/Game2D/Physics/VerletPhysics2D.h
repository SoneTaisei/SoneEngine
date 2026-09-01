#pragma once
#include "Core/Utility/Vector3.h"
#include "Core/Utility/Structs.h"
#include <vector>

class MapChip2D;

/// <summary>
/// Verlet物理のノード（質点）
/// 速度変数を持たず pos - prevPos が暗黙の速度になる
/// </summary>
struct VerletNode {
    Vector3 pos = { 0.0f, 0.0f, 0.0f };      // 現在位置（ワールド、z=0固定）
    Vector3 prevPos = { 0.0f, 0.0f, 0.0f };  // 前ステップ位置
    float invMass = 1.0f;                    // 質量の逆数。0なら固定ノード（アンカー）
    float radius = 0.1f;                     // 円コリジョン半径
};

/// <summary>
/// 汎用Verlet物理ソルバー（2D / z=0平面）
/// 鎖・ロープ等の位置ベース物理に流用できるstaticユーティリティ
/// 固定タイムステップ(1/60)前提。乱数・時刻は一切使わない（リプレイ再現性のため）
/// </summary>
class VerletPhysics2D {
public:
    /// <summary>
    /// Verlet積分（全ノード）
    /// invMass == 0 のノードは動かさず速度もリセットする
    /// </summary>
    static void Integrate(std::vector<VerletNode>& nodes, const Vector3& gravity, float damping, float dt);

    /// <summary>
    /// 距離制約を1本解く（invMassの重み付けで位置補正）
    /// </summary>
    static void SolveDistanceConstraint(VerletNode& a, VerletNode& b, float restLength);

    /// <summary>
    /// ノード（円） vs マップチップ(kBlock)のAABB押し出し
    /// 最近接点方式なので角にも滑らかに巻き付く
    /// friction > 0 の場合、接触時に暗黙速度を削る（prevPosをposへ近づける）
    /// </summary>
    static void CollideNodeWithMap(VerletNode& node, MapChip2D* map, float friction);

    /// <summary>
    /// ノード（円） vs 任意AABBの押し出し（プレイヤー等の動くコライダ用）
    /// 接触した場合 true を返す
    /// </summary>
    static bool CollideNodeWithAABB(VerletNode& node, const AABB2D& aabb, float friction);

    /// <summary>
    /// ノードに速度を注入する（prevPos操作。接触したコライダの速度を伝える用途）
    /// influence: 0〜1（1でvelocityをそのまま加算）
    /// </summary>
    static void ApplyVelocity(VerletNode& node, const Vector3& velocity, float dt, float influence);

    /// <summary>
    /// 全ノードの暗黙速度をゼロにする（prevPos = pos）
    /// アンカーのワープ・リスポーン時の暴れ防止
    /// </summary>
    static void ResetVelocities(std::vector<VerletNode>& nodes);

    /// <summary>
    /// 誤差蓄積の保険として全ノードの z を 0 に矯正する
    /// </summary>
    static void ClampToPlaneZ(std::vector<VerletNode>& nodes);

private:
    // 接触応答の共通処理（押し出し後の摩擦適用）
    static void ApplyContactFriction(VerletNode& node, float friction);
};
