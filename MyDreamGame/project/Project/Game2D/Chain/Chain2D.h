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
/// 鎖（チェーン）
/// VerletPhysics2Dを適用した節ノード列 + 縦横リンクモデルの交互描画
/// アンカー（固定端）から垂れ下がり、地形に巻き付き、プレイヤーに反応する
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

    // --- パラメータ ---
    const ChainParams& GetParams() const { return params_; }
    void SetParams(const ChainParams& params);

    // --- エディタ/ヒエラルキー用 ---
    std::vector<Object3D*> GetLinkObjects() const;
    void DrawImGui();

private:
    // ノード列を初期垂下姿勢で構築する
    void BuildNodes();
    // リンク描画用Object3Dを構築する（節数変更時に呼び直す）
    void BuildLinkObjects();
    // ノード位置からリンクモデルのSRTを更新する
    void UpdateLinkTransforms();
    // 1サブステップ分の物理更新
    void StepSimulation(float dt, MapChip2D* map, Player2D* player);

    std::string name_ = "Chain";
    ChainParams params_;
    float restLength_ = 0.25f; // 節間隔 = totalLength_ / (nodeCount_ - 1)

    std::vector<VerletNode> nodes_; // フラット管理。これが唯一の真実
    Vector3 anchorPos_ = { 0.0f, 0.0f, 0.0f };
    const Vector3* anchorFollow_ = nullptr;
    const Vector3* endFollow_ = nullptr;

    // 描画用（縦リンク・横リンクを交互に割り当てる）
    std::vector<std::unique_ptr<Object3D>> linkObjs_;
    Model* modelTate_ = nullptr;
    Model* modelYoko_ = nullptr;
    ID3D12Device* device_ = nullptr;
};
