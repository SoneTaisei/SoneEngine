#pragma once
#include "Core/Utility/Vector3.h"
#include <memory>
#include <string>
#include <vector>

class Player2D;
class Chain2D;

/// <summary>
/// プレイヤーと鎖の接続管理
/// ソケット（手のジョイント）座標の毎フレーム同期と、鎖を拾う・落とす入力処理を担当する
/// GameScene::Update で player_->UpdateWithMap() の直後、chain->Update() の前に呼ぶこと
/// </summary>
class PlayerChainController {
public:
    // gaikotu 既存ジョイント（右手）。左手なら "LeftHand_Dummy_012"
    static constexpr const char* kSocketJointName = "RightHand_Dummy_017";

    void Initialize(Player2D* player);

    /// <summary>
    /// 拾う/落とす入力の処理と、保持中の鎖へのソケット座標同期
    /// （拾う/落とすキーは K。リプレイ録画対象の'C'スロットなので再生時も再現される）
    /// </summary>
    void Update(std::vector<std::unique_ptr<Chain2D>>& chains);

    /// <summary>
    /// 保持状態を解除する（シーンリセット・リプレイ再生開始時に呼ぶ。鎖側のモードは触らない）
    /// </summary>
    void Release() { heldChain_ = nullptr; }

    Chain2D* GetHeldChain() const { return heldChain_; }
    bool IsHolding() const { return heldChain_ != nullptr; }
    const Vector3& GetSocketWorld() const { return lastSocketWorld_; }
    bool IsSocketValid() const { return socketValid_; }

    void DrawImGui();

private:
    // ソケット（手のジョイント）のワールド座標を取得。ジョイントが無ければプレイヤー座標で代用
    Vector3 ComputeSocketWorld();

    Player2D* player_ = nullptr;
    Chain2D* heldChain_ = nullptr;
    Vector3 lastSocketWorld_ = { 0.0f, 0.0f, 0.0f };
    bool socketValid_ = false;
    bool loggedSocketState_ = false; // ジョイント検出結果を初回だけログに出す
    float pickupRange_ = 1.5f;       // 拾える距離（プレイヤー中心からノードまで）
};
