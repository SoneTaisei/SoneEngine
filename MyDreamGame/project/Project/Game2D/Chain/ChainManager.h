#pragma once
#include "Game2D/Chain/Chain2D.h"
#include "Game2D/Treasure/Treasure2D.h"
#include "Core/Utility/Vector3.h"
#include <memory>
#include <string>
#include <vector>

class Player2D;
class MapChip2D;
class Object3D;

/// <summary>
/// 鎖全体の管理（ユニット制）
/// - playerChain_  : プレイヤーの手から下がる鎖1本。ユニット数は chainLength_ と Reconcile で同期し、
///                   増えた分は手元から繰り出される。末端はお宝（重り）
/// - worldChains_  : マップ配置の吊り鎖（拾うとユニットをもらえる。使い切ると休眠しリセットで復活）
///                   現在は配置なし。AddWorldChain() がマップ配置やレベルデータ生成の入口
/// - droppedChains_: 外して落とした自由鎖（拾うと個数が戻る）
/// - treasure_     : プレイヤー鎖の末端ノードに描画されるお宝（表示専用）
/// 操作は分離：K = 拾う（範囲内に鎖がなければ何もしない）/ J・S = unitsPerAction_ ユニットまとめて外す
/// </summary>
class ChainManager {
public:
    // gaikotu 既存ジョイント（右手）。左手なら "LeftHand_Dummy_012"
    static constexpr const char* kSocketJointName = "RightHand_Dummy_017";

    /// <summary>
    /// 初期化。プレイヤー鎖とお宝を生成する（吊り鎖は生成しない）
    /// </summary>
    void Initialize(Player2D* player);

    /// <summary>
    /// マップ配置の吊り鎖を追加する（将来のマップ ChipType / レベルデータ生成用の入口）
    /// 必ず Initialize() の後に呼ぶこと（Initialize は worldChains_ をクリアする）
    /// </summary>
    void AddWorldChain(const Vector3& anchorPos, int units, const std::string& name = "WorldChain");

    /// <summary>
    /// 鎖アクション入力の処理。player_->UpdateWithMap() の直後に呼ぶ
    /// K = 拾う（範囲内に鎖がなければ何もしない）/ J・S(下) = 外す
    /// どのキーもリプレイ録画対象スロット（K='C'、J='D'、S='S'）のため再生時も再現される
    /// 注意: 'C'はCtrl、'D'はShiftとスロット共有。録画中にそれらを押すと再生時に幻の入力になり得る
    /// </summary>
    void HandleInput();

    /// <summary>
    /// chainLength_（個数の正）とプレイヤー鎖のユニット数を照合し、差分だけ伸縮する
    /// 増える分は繰り出しキューに積まれ手元から出てくる。ChainItemBlock・拾う・デバッグ操作は AddChainLength を呼ぶだけでよい
    /// </summary>
    void Reconcile();

    /// <summary>
    /// 鎖の物理更新（プレイヤー鎖 → 吊り鎖 → 自由鎖の順）とお宝の位置同期
    /// </summary>
    void Update(float dt, MapChip2D* map);

    void Draw();

    /// <summary>
    /// プレイ開始・リプレイ再生開始時のリセット
    /// （自由鎖を全消去し、個数を初期値へ、吊り鎖とプレイヤー鎖を初期姿勢へ）
    /// </summary>
    void ResetAll();

    /// <summary>
    /// 巻き戻し明けの処理（自由鎖の消去 + 速度リセット。巻き戻し中の鎖アクションは再現しない割り切り）
    /// </summary>
    void OnRewindEnd();

    std::vector<Object3D*> GetLinkObjects() const;
    void DrawImGui();

    Chain2D* GetPlayerChain() const { return playerChain_.get(); }
    const Vector3& GetSocketWorld() const { return lastSocketWorld_; }

    // --- お宝（ゴール判定・敗北判定・カメラ用のフック） ---
    Treasure2D* GetTreasure() const { return treasure_.get(); }
    Vector3 GetTreasurePosition() const { return treasure_ ? treasure_->GetPosition() : Vector3{ 0.0f, 0.0f, 0.0f }; }
    /// <summary>お宝がプレイヤー鎖につながっているか（将来「鎖が途切れる=敗北」用。現状は常に true）</summary>
    bool IsTreasureConnected() const { return playerChain_ && playerChain_->GetEndWeight().enabled; }

private:
    // ソケット（手のジョイント）のワールド座標を取得。ジョイントが無ければプレイヤー座標で代用
    Vector3 ComputeSocketWorld();
    // 範囲内の鎖を拾う（落ちている自由鎖 → 吊り鎖の順）。拾えたら true
    bool TryPickup();
    // unitsPerAction_ ユニットをつながったまま外して自由鎖として世界に落とす
    void DetachUnits();
    // params_ のお宝設定をプレイヤー鎖の末端と表示に反映する
    void ApplyTreasureParams();
    // お宝の表示位置をプレイヤー鎖の末端に合わせる
    void SyncTreasureTransform();

    Player2D* player_ = nullptr;
    ChainParams params_; // 共有パラメータ（ChainConfigからロード）

    std::unique_ptr<Chain2D> playerChain_;
    std::vector<std::unique_ptr<Chain2D>> worldChains_;
    std::unique_ptr<Treasure2D> treasure_;

    // 外して落とした自由鎖（unitWorth = 拾った時に戻る個数）
    struct DroppedChain {
        std::unique_ptr<Chain2D> chain;
        int unitWorth = 1;
    };
    std::vector<DroppedChain> droppedChains_;

    Vector3 lastSocketWorld_ = { 0.0f, 0.0f, 0.0f };
    bool socketValid_ = false;
    bool loggedSocketState_ = false;
    int initialChainLength_ = 3; // シーン開始時の個数（リセット時に戻す）
    int droppedCounter_ = 0;     // 自由鎖の命名用連番
};
