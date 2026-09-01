#pragma once
#include <string>

/// <summary>
/// 鎖のパラメータ（チップ1.0単位系・ユニット制）
/// 鎖の長さは「ユニット数」で決まる：節数 = 1 + ユニット数 × nodesPerUnit_、
/// 節間隔 restLength = unitLength_ / nodesPerUnit_（デフォルト 1.0/4 = 0.25）
/// </summary>
struct ChainParams {
    // --- ユニット系 ---
    int initialUnits_ = 3;             // 初期ユニット数（プレイヤー鎖は chainLength_ と同期）
    float unitLength_ = 1.0f;          // 1ユニットの長さ（チップ何枚分か）
    int nodesPerUnit_ = 4;             // 1ユニットあたりのノード数
    int maxUnits_ = 8;                 // プレイヤー鎖の上限（重すぎて詰まないように）
    int minUnits_ = 1;                 // 下限。最後の1本は外せない
    int unitsPerAction_ = 2;           // 一度のK操作で外す/もらうユニット数（つながったまま着脱される）
    float pickupRadius_ = 0.5f;        // 拾う判定（プレイヤーのAABBから鎖ノードまでの距離）
    float payoutSpeed_ = 6.0f;         // 手元からの繰り出し速度（チップ/秒）。2ユニット≒0.33秒

    // --- 物理 ---
    float gravity_ = -35.0f;           // プレイヤーの gravity_ と同値から開始
    float damping_ = 0.99f;            // 減衰（1.0だと永久に揺れる）
    int iterations_ = 15;              // 制約反復回数（伸びるなら増やすかサブステップ）
    int subSteps_ = 1;                 // サブステップ分割数（硬くしたい時に2〜4へ）
    float nodeRadius_ = 0.1f;          // ノードの円コリジョン半径
    float friction_ = 0.5f;            // 接触摩擦（接触時のみ暗黙速度を削る）
    float playerVelInfluence_ = 0.35f; // 接触時にプレイヤー速度を鎖へ伝える割合
    int rootCollisionSkip_ = 2;        // 手持ち中に地形判定から除外する根元ノード数（壁張り付き時のジッタ防止）

    // --- お宝（重り）: プレイヤー鎖の末端ノード ---
    float treasureMass_ = 5.0f;        // 質量（invMass = 1/mass。5なら鎖側が8割動く）
    float treasureRadius_ = 0.3f;      // 半径（ノード0.1の3倍。段差に引っかかる）
    float treasureFriction_ = 0.8f;    // 地形との摩擦（引きずると渋い）
    bool treasureIgnorePlayer_ = false; // お宝をプレイヤー衝突から外す（狭い通路で押されて困る時用）

    // --- 見た目 ---
    float linkThickness_ = 1.0f;       // リンクモデルの太さ倍率
    float linkOverlap_ = 1.6f;         // 節間隔に対するリンクモデル長の倍率（重なり量）
};

/// <summary>
/// 鎖パラメータのJSON保存・読み込み（PlayerConfigと同じ流儀）
/// </summary>
class ChainConfig {
public:
    // デフォルトの保存先
    static const std::string kDefaultFilePath;

    static void Save(const ChainParams& params, const std::string& filepath);
    static void Load(ChainParams& params, const std::string& filepath);
};
