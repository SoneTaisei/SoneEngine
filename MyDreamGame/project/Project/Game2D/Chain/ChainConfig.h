#pragma once
#include <string>

/// <summary>
/// 鎖のパラメータ（チップ1.0単位系）
/// </summary>
struct ChainParams {
    int nodeCount_ = 12;               // 節数（多いほど滑らか・伸びやすい）
    float totalLength_ = 3.0f;         // 全長（チップ何枚分か）
    float gravity_ = -35.0f;           // プレイヤーの gravity_ と同値から開始
    float damping_ = 0.99f;            // 減衰（1.0だと永久に揺れる）
    int iterations_ = 15;              // 制約反復回数（伸びるなら増やすかサブステップ）
    int subSteps_ = 1;                 // サブステップ分割数（硬くしたい時に2〜4へ）
    float nodeRadius_ = 0.1f;          // ノードの円コリジョン半径
    float friction_ = 0.5f;            // 接触摩擦（接触時のみ暗黙速度を削る）
    float playerVelInfluence_ = 0.35f; // 接触時にプレイヤー速度を鎖へ伝える割合
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
