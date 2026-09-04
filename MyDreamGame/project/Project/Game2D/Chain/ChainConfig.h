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

    // --- お宝（重り）の物理: プレイヤー鎖の末端ノード ---
    float treasureMass_ = 5.0f;        // 質量（鎖物理の invMass = 1/mass、スピンの振りにくさにも使う）
    float treasureRadius_ = 0.3f;      // 当たり半径（ノード0.1の3倍。段差に引っかかる。見た目のスケールとは独立）
    float treasureFriction_ = 0.8f;    // 地形との摩擦（引きずると渋い）
    bool treasureIgnorePlayer_ = false; // お宝をプレイヤー衝突から外す（狭い通路で押されて困る時用）
    bool heldChainPlayerCollision_ = false; // 持っている鎖とプレイヤーの当たり判定（false: 判定なし。回した重りや鎖が体に引っかからない）

    // --- ちぎれ（鎖が伸び切ったらミス） ---
    bool tearEnabled_ = true;          // 宝石が地形に引っかかったまま手元が離れ、鎖が伸び切り続けたらちぎれてミスになる
    float tearStretchRatio_ = 1.4f;    // ちぎれる伸び（直線距離 ÷ 鎖の実長）。1.0 が伸び切った状態
    float tearStuckSpeed_ = 1.5f;      // 宝石の速さがこれ未満（引っかかって動けない）の時だけ伸びを数える（チップ/秒）
    float tearGraceTime_ = 0.35f;      // 「伸びている かつ 宝石が止まっている」がこの秒数続いたらちぎれる（物理の一時的な伸びは無視）

    // --- お宝の見た目（物理とは独立。宝石モデルへの差し替えはここを書き換えるだけ） ---
    std::string treasureModelDir_ = "resources/Object/Original/sphere";
    std::string treasureModelFile_ = "sphere.obj";
    float treasureScale_ = 0.3f;       // 表示スケール（sphere.obj は半径1.0なので 0.3 で物理半径と一致）

    // --- スピンジャンプ（木の板の上で構え、ピンと張った鎖を自分で振り、離すと鎖ごと飛び、プレイヤーも同じ方向へ飛ぶ） ---
    float spinRadiusMax_ = 5.0f;       // 回転半径の上限
    float spinRadiusRatio_ = 1.0f;     // 回転半径 = 鎖の実長 × これ（1.0 で節間隔ちょうど。下げると縮めた棒になり離した瞬間に伸びる）
    float holdOffset_ = 0.9f;          // W で宝石を掲げている間の、手から真上の宝石までの距離（鎖はその間に畳まれる）
    float throwOutTime_ = 0.2f;        // A/D で投げてから棒が鎖の実長まで伸び切るまでの秒数
    float throwAngleDeg_ = 180.0f;     // 投げ始めの角度（真下=0。180 で真上＝頭上から振り下ろす、90 で真横）
    float throwOmega_ = 2.0f;          // 投げた瞬間の角速度（rad/s。投げた方向へ回り続ける勢い）
    float swingStrength_ = 40.0f;      // A/Dで振る力。角加速度 = これ ÷ (宝石の質量 + 鎖の質量)
                                       // 40: 押しっぱなしでは弱く(3ユニットで3.5u/s)、交互に漕ぐと約3秒で上限到達
    float swingDamping_ = 0.25f;       // 振りの減衰（1/秒。漕がないと徐々に止まる）
    float chainMassPerUnit_ = 0.5f;    // 鎖1ユニットあたりの質量（宝石の質量に加算。長いほど振りにくい）
    float weightThrowScale_ = 1.0f;    // 離した時に鎖と重りへ与える速度の倍率（角速度 × 半径 × これ）
    float pullTransfer_ = 0.9f;        // 離した時にプレイヤーが飛ぶ速さ = 重りの速さ × これ
    float launchMaxJumpRatio_ = 1.0f;  // 引く速さの上限 = 通常ジャンプ初速 × これ（1.0 で通常ジャンプより高くは飛べない）
    float launchMinUpward_ = 0.35f;    // 引く方向の最低上向き成分（真横で引かれても床に貼り付かない）
    float spinMoveFactor_ = 0.0f;      // 構え中の移動速度倍率（0 で移動不可。A/D は振りに使う）
    float spinCooldown_ = 0.4f;        // 引かれた後のクールダウン（秒。着地でも解除）
    bool spinAnywhere_ = false;        // false: 木の板（ThinPlatformBlock）の上に立っている時だけ回せる（既定）。true: どこでも回せる（調整用）

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
