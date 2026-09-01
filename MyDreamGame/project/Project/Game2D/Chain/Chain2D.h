#pragma once
#include "Game2D/Physics/VerletPhysics2D.h"
#include "Game2D/Chain/ChainConfig.h"
#include "Core/Utility/Vector3.h"
#include <d3d12.h>
#include <memory>
#include <string>
#include <vector>

class MapChip2D;
class Player2D;
class Object3D;
class Model;

/// <summary>
/// 鎖の根元（アンカー）の状態
/// </summary>
enum class ChainAnchorMode {
    kWorld,  // 根元をワールド座標に固定（吊り下げ装飾）
    kSocket, // 根元を外部ソケット（プレイヤーの手など）へ毎フレーム追従（持たれている状態）
    kFree,   // 固定なし（地面に落ちている状態）
};

/// <summary>
/// 末端の重り（お宝）設定。お宝 = 鎖の最後のノードそのもの
/// 距離制約は invMass 重み付きなので、重い末端は動きにくく軽い鎖側が引き寄せられる
/// </summary>
struct EndWeight {
    bool enabled = false;
    float mass = 5.0f;         // invMass = 1/mass（5なら鎖側が8割動く）
    float radius = 0.3f;       // 円コリジョン半径（段差に引っかかる）
    float friction = 0.8f;     // 地形との摩擦（末端のみ。引きずると渋い）
    bool ignorePlayer = false; // プレイヤー衝突から除外する
};

/// <summary>
/// 鎖（チェーン）
/// VerletPhysics2Dを適用した節ノード列 + 縦横リンクモデルの交互描画
/// アンカー（固定端）から垂れ下がり、地形に巻き付き、プレイヤーに反応する
/// ユニットの追加は「手元からの繰り出し」で行う（先頭セグメントの自然長を少しずつ伸ばし、1節分に達したら手元に挿入）
/// </summary>
class Chain2D {
public:
    /// <summary>
    /// 初期化。anchorPosから真下に垂れた状態で生成する
    /// </summary>
    void Initialize(const Vector3& anchorPos, const ChainParams& params, const std::string& name = "Chain");

    /// <summary>
    /// 更新（プレイヤー位置確定後に呼ぶこと）
    /// dt は TimeManager::GetDeltaTime() をシーン側から渡す流儀（PlayerPhysics::Updateと同じ）
    /// </summary>
    void Update(float dt, MapChip2D* map, Player2D* player);

    /// <summary>
    /// 描画（modelCommon_->PreDraw() 後に呼ぶこと）
    /// </summary>
    void Draw();

    /// <summary>
    /// 暗黙速度をゼロにする（アンカーのワープ・リスポーン時の鞭化防止）
    /// </summary>
    void ResetDynamics();

    /// <summary>
    /// 初期垂下姿勢に戻す（プレイ開始・リプレイ再生開始時の再現性確保用）
    /// </summary>
    void ResetToInitial();

    // --- アンカー（固定端）操作 ---
    void SetAnchorPosition(const Vector3& pos) { anchorPos_ = pos; }
    const Vector3& GetAnchorPosition() const { return anchorPos_; }

    // --- アンカーモード（拾う・落とす対応） ---
    ChainAnchorMode GetAnchorMode() const { return anchorMode_; }

    /// <summary>
    /// アンカーモードを切り替える（kFree=落とす、kSocket=持つ、kWorld=ワールド固定）
    /// </summary>
    void SetAnchorMode(ChainAnchorMode mode);

    /// <summary>
    /// kSocket時のソケット座標同期。毎フレーム、Update() の前に呼ぶこと
    /// （シミュレーションは z=0 で行うため z は無視される）
    /// </summary>
    void SyncSocket(const Vector3& socketWorld);

    /// <summary>
    /// アンカーを外部座標に追従させる（動くブロックへの取り付け等。nullptrで解除）
    /// </summary>
    void SetAnchorFollowTarget(const Vector3* target) { anchorFollow_ = target; }

    // --- 将来のプレイヤー掴み・吊りオブジェクト用フック ---
    /// <summary>
    /// 終端ノードを外部座標に拘束する（プレイヤーが掴んだ状態等。nullptrで解除）
    /// </summary>
    void SetEndFollowTarget(const Vector3* target);

    int GetNodeCount() const { return static_cast<int>(nodes_.size()); }
    const Vector3& GetNodePosition(int index) const { return nodes_[index].pos; }
    Vector3 GetEndPosition() const { return nodes_.empty() ? anchorPos_ : nodes_.back().pos; }

    // --- ユニット操作（鎖の伸縮。1ユニット = nodesPerUnit_ ノード） ---
    /// <summary>現在のユニット数（繰り出し待ちのノードも含めて数える。含めないと Reconcile が無限に伸ばす）</summary>
    int GetUnitCount() const;

    /// <summary>ユニット数を目標値へ合わせる（増える分は繰り出しキューへ、減る分はアンカー側から削除）</summary>
    void SetUnitCount(int units);

    /// <summary>
    /// ユニットを繰り出しキューに積む。毎フレーム payoutSpeed_ で手元から少しずつ出てくる
    /// </summary>
    void QueueUnits(int units);

    /// <summary>
    /// アンカー側にユニットを即時挿入する（アンカーと隣ノードの間に線形補間で配置）
    /// 通常は QueueUnits を使う。即時に長さが必要な初期化用
    /// </summary>
    void AddUnitsAtAnchor(int units);

    /// <summary>
    /// アンカー側からユニットを切り離して返す（pos/prevPos維持 = 落下が連続的に見える）
    /// 繰り出し待ちのノードを先に消費し、足りない分だけ実ノードを削除する
    /// アンカーノードと（重りが有効なら）末端ノードは必ず残る
    /// 返り値は先頭にアンカー複製を含む 1 + 削除ノード数 で、そのまま InitializeFromNodes に渡せる
    /// </summary>
    std::vector<VerletNode> RemoveUnitsAtAnchor(int units);

    /// <summary>繰り出し中か（キューが残っている、または先頭セグメントが伸び切っていない）</summary>
    bool IsPayingOut() const;
    int GetPendingNodeCount() const { return pendingNodes_; }

    /// <summary>
    /// 切り離したノード列から自由鎖（kFree）を生成する（外した鎖を世界に落とす用）
    /// </summary>
    void InitializeFromNodes(std::vector<VerletNode>&& nodes, const ChainParams& params, const std::string& name);

    /// <summary>いずれかのノードが point から radius 以内にあるか（拾う判定）</summary>
    bool FindNearestNode(const Vector3& point, float radius, int* outIndex = nullptr) const;

    /// <summary>
    /// いずれかのノード（円）が AABB から margin 以内にあるか（拾う判定の本命）
    /// 中心点距離ではなく体の箱からの距離で判定するため、足元に横たわる鎖も見た目通りに拾える
    /// </summary>
    bool FindNearestNodeToAABB(const AABB2D& box, float margin, int* outIndex = nullptr) const;

    /// <summary>
    /// 手持ち中（kSocket）にプレイヤー衝突から除外する根元ノード数
    /// （根元がプレイヤーに追従するため、除外しないと毎フレーム押し合う）
    /// </summary>
    void SetPlayerCollisionSkipCount(int count) { playerCollisionSkip_ = count; }

    // --- 末端の重り（お宝） ---
    void SetEndWeight(const EndWeight& weight);
    const EndWeight& GetEndWeight() const { return endWeight_; }

    void SetPayoutSpeed(float chipsPerSecond) { params_.payoutSpeed_ = (chipsPerSecond < 0.1f) ? 0.1f : chipsPerSecond; }

    // --- パラメータ ---
    const ChainParams& GetParams() const { return params_; }
    void SetParams(const ChainParams& params);

    // --- エディタ/ヒエラルキー用 ---
    std::vector<Object3D*> GetLinkObjects() const;
    void DrawImGui();

private:
    // ノード列を初期垂下姿勢で構築する
    void BuildNodes();
    // リンク描画用Object3Dを count 本まで確保する（既存分は再生成しない。繰り出し中の定数バッファ生成を避ける）
    void EnsureLinkCapacity(size_t count);
    // ノード位置からリンクモデルのSRTを更新する
    void UpdateLinkTransforms();
    // 1サブステップ分の物理更新
    void StepSimulation(float dt, MapChip2D* map, Player2D* player);
    // 手元からの繰り出し（先頭セグメントの自然長を伸ばし、1節分に達したら手元にノードを挿入）
    void UpdatePayout(float dt);
    // 末端ノードへ重り設定を適用する（ノード再構築で invMass が 1 に戻るため都度呼ぶ）
    void ApplyEndWeight();

    std::string name_ = "Chain";
    ChainParams params_;
    float restLength_ = 0.25f; // 節間隔 = unitLength_ / nodesPerUnit_

    std::vector<VerletNode> nodes_; // フラット管理。これが唯一の真実
    Vector3 anchorPos_ = { 0.0f, 0.0f, 0.0f };
    const Vector3* anchorFollow_ = nullptr;
    const Vector3* endFollow_ = nullptr;

    // アンカーモード（現在値と、シーンリセット時に戻す初期値）
    ChainAnchorMode anchorMode_ = ChainAnchorMode::kWorld;
    ChainAnchorMode initialMode_ = ChainAnchorMode::kWorld;
    Vector3 initialAnchorPos_ = { 0.0f, 0.0f, 0.0f };

    // 手持ち中にプレイヤー衝突から除外する根元ノード数（0なら全ノード判定）
    int playerCollisionSkip_ = 0;

    // 繰り出し（スプール）
    int pendingNodes_ = 0;    // まだ繰り出していないノード数
    float headRest_ = 0.25f;  // 先頭セグメント(node0-node1)の現在の自然長。0〜restLength_

    // 末端の重り（お宝）
    EndWeight endWeight_;

    // 描画用リンク。添字 j は「末端からの距離」（末端リンクが j=0）
    // 縦横のパリティを末端基準にすると、手元での増減で既存リンクのモデルが入れ替わらずチカチカしない
    std::vector<std::unique_ptr<Object3D>> linkObjs_;
    bool hideHeadLink_ = false; // 繰り出し直後の極短い先頭リンクは描かない
    Model* modelTate_ = nullptr;
    Model* modelYoko_ = nullptr;
    ID3D12Device* device_ = nullptr;
};
