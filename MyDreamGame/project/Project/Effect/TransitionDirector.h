#pragma once
#include "Core/Utility/Vector3.h"
#include "Game2D/Chain/ChainConfig.h"
#include "Game2D/Chain/Chain2D.h"
#include <memory>
#include <string>
#include <vector>

class Object3D;
class Treasure2D;
class ChainManager;
class MapChip2D;
class Player2D;
class GameCamera;

/// <summary>
/// ステージクリア遷移の演出監督（シングルトン。シーンをまたいで宝石と鎖を持ち越す）
///
/// ゴール側（GameScene）: StartStageClear()
///   clearStyle 0（既定・シネマティック）:
///     ZoomIn     カメラがプレイヤーに寄る（追従を止めて座標と正射影サイズを直接動かす）
///     Joy        その場で跳ねて一回転（脱出を喜ぶ）。鎖は手に付いたまま揺れる
///     EnterGoal  ゴールへ歩き、背を向けて奥へ小さくなって消える。鎖は手元へ巻き取られる
///     FlyBy      大きくなってカメラの前を右から左へ飛び抜ける（宝石と鎖を引き連れて）
///     IrisClose  画面中央から黒い円で絞って暗転 → Covered
///   clearStyle 1（スポットライト）:
///     IrisClose  穴を宝石へ絞りながら背景板をフェードイン → Hold → Descend（黒の上で宝石と鎖を下へ運ぶ）→ IrisFinish → Covered
///   Covered    ConsumeCoveredEvent() が1回 true になる → シーン側が ChangeScene する
/// 次シーン側（GameScene::Initialize）: StartStageOpen()
///   OpenDescend 黒の中、持ち越した宝石と鎖が画面上端から手元へ降りて着地。ゲーム側の鎖に引き継ぐ
///   OpenReveal  宝石を中心に穴が開き、背景板がフェードアウトしてステージが現れる
///
/// 見た目の構成（すべて通常の3Dパス。深度で前後が決まる。スプライト不使用・Engine 無変更）
///   黒板(アニュラス)  z=-0.9  穴の外を隠す。スケール s で穴の半径 = s、外径 = 20000s
///   飛び抜ける本人    z=-2.0  プレイヤーの Object3D をそのまま大きくして手前に置く
///   遷移用の宝石と鎖  z=-0.8  穴の中で見えるのはこれだけ
///   黒い背景板        z=-0.6  ブロックの前面(-0.5)・プレイヤーより手前。アルファでフェード
///   ステージ          z=0
/// プレイヤーの演技は Player2D のモデル(Object3D)の座標・回転・スケールを、通常の更新の後から上書きして行う
/// </summary>
class TransitionDirector {
public:
    enum class Phase {
        kNone,
        // ゴール側（シネマティック）
        kZoomIn,
        kJoy,
        kEnterGoal,
        kFlyBy,
        // ゴール側（共通 / スポットライト）
        kIrisClose,
        kHold,
        kDescend,
        kIrisFinish,
        kCovered,
        // 次シーン側
        kOpenDescend,
        kOpenReveal,
    };

    // ParameterManager の "Transition" グループから読む（ImGui のパラメータ一覧で調整可）
    struct Params {
        int clearStyle_ = 0;            // 0: シネマティック（寄る→喜ぶ→奥へ→飛び抜け→暗転） / 1: スポットライトで宝石を下へ運ぶ
        // --- シネマティック ---
        float zoomTime_ = 0.7f;         // カメラが寄る時間
        float zoomScale_ = 2.2f;        // 寄った時の拡大率
        float joyTime_ = 1.2f;          // 喜ぶ時間
        float hopHeight_ = 0.6f;        // 跳ねる高さ
        int hopCount_ = 2;              // 跳ねる回数
        bool joySpin_ = true;           // 喜ぶ間に一回転する
        float enterGoalTime_ = 0.7f;    // ゴールへ入る時間
        float enterGoalYaw_ = 3.14159f; // 奥へ向く時の向き（Y回転、rad。モデルの正面次第で 0 に変える）
        float enterGoalDepth_ = 1.5f;   // 奥へ入る距離（+z）
        float flyTime_ = 1.1f;          // 飛び抜ける時間
        float flyScale_ = 3.0f;         // 飛び抜ける時の大きさ
        float flyZ_ = -2.0f;            // 飛び抜ける時の z（カメラ z=-10 の手前側）
        float flyTilt_ = 0.6f;          // 飛ぶ姿勢の傾き（Z回転、rad）
        float flyArcHeight_ = 1.0f;     // 飛行の弧の高さ
        float flyMargin_ = 2.5f;        // 画面端の外側から出入りする余白
        // --- 円と背景板 ---
        float irisCloseTime_ = 0.6f;    // 絞る時間（easeInOut。背景板のフェードインも同じ時間）
        float irisHoldRadius_ = 1.5f;   // スポットライトの穴の半径（宝石の周り、チップ）
        float holdTime_ = 0.2f;         // 溜め（スポットライト）
        float descendTime_ = 0.8f;      // 降下時間（スポットライト。easeIn。加速度が重力の maxAnchorAccelRatio_ 倍を超える場合は自動で延びる）
        float irisFinishTime_ = 0.2f;   // 穴を閉じ切る時間（スポットライト）
        float landTime_ = 0.8f;         // 次シーンで宝石と鎖が上から降りて手元に着くまでの時間（easeOut）
        float irisOpenTime_ = 0.8f;     // 着地後に穴が開き背景板が消えるまでの時間（easeOut）
        float irisZ_ = -0.9f;           // 黒板の z（遷移用の鎖・宝石より手前）
        float backdropZ_ = -0.6f;       // 背景板の z（ブロック前面 -0.5 より手前、遷移用の鎖 -0.8 より奥）
        float carryZ_ = -0.8f;          // 遷移用の鎖の描画 z（宝石はさらに 0.05 手前）
        float irisMinScale_ = 0.003f;   // 閉じ切った時のスケール
        float descendMargin_ = 0.5f;    // 画面端からさらに余分に運ぶ距離
        float maxAnchorAccelRatio_ = 0.9f; // 降下の加速度上限（重力比）
        bool carryChainLength_ = true;  // 鎖の個数を次のステージへ持ち越す
    };

    static TransitionDirector* GetInstance();

    /// <summary>
    /// ゴール側の開始。プレイヤー鎖と宝石を複製して遷移側が所有し、ゲーム側の鎖を非表示にする
    /// player はモデルの演技とカメラの寄り先、camera は寄り・戻しに使う（どちらも非所有）
    /// </summary>
    void StartStageClear(ChainManager* chains, MapChip2D* map, Player2D* player, GameCamera* camera);

    /// <summary>
    /// 次シーン側の開始（GameScene::Initialize で呼ぶ）。覆い切った状態（Covered）でなければ何もしない
    /// 持ち越した鎖を画面上端の上に用意し、黒の中を手元へ降ろして着地させてから穴を開く
    /// </summary>
    void StartStageOpen(ChainManager* chains, const Vector3& playerPos, float orthoWidth, float orthoHeight);

    void Update(float dt);
    /// <summary>modelCommon->PreDraw() の後（鎖の描画の後）に呼ぶ</summary>
    void Draw();

    /// <summary>黒で覆い切った瞬間に1回だけ true（シーン側はこれを見て ChangeScene する）</summary>
    bool ConsumeCoveredEvent();

    bool IsPlaying() const { return phase_ != Phase::kNone; }
    /// <summary>黒い円で覆っている最中か（パーティクル等を描かない判断に使う）</summary>
    bool IsCovering() const;
    bool IsCovered() const { return phase_ == Phase::kCovered; }
    /// <summary>演出がカメラを直接動かしている間 true（シーン側は追従ターゲットを再設定しない）</summary>
    bool IsCameraControlled() const { return cameraControlled_; }
    /// <summary>次シーンへ持ち越す鎖があるか</summary>
    bool HasCarry() const { return carryChain_ != nullptr || (hasCarryData_ && carryUnits_ > 0); }
    /// <summary>持ち越している鎖のユニット数（繰り出し待ちも含む）</summary>
    int GetCarryUnits() const { return carryUnits_; }
    Phase GetPhase() const { return phase_; }
    const Params& GetParams() const { return params_; }

    /// <summary>シーン破棄時に呼ぶ。覆い切った直後（次シーンへ持ち越す途中）以外は演出を捨てる</summary>
    void OnSceneDestroyed(ChainManager* chains);
    /// <summary>演出を中断して全部捨てる</summary>
    void Abort();

    std::vector<Object3D*> GetObjects() const;
    void DrawImGui();

private:
    TransitionDirector() = default;
    ~TransitionDirector();
    TransitionDirector(const TransitionDirector&) = delete;
    TransitionDirector& operator=(const TransitionDirector&) = delete;

    void LoadParams();
    void EnsureObjects();
    // プレイヤー鎖を複製して遷移側の鎖にする（持ち越し用のパラメータも控える）
    void CloneCarry(ChainManager* chains);
    // 控えたパラメータから持ち越し用の鎖を作り直す（シネマティックで巻き取った後の次シーン用）
    void BuildCarryFromData(const Vector3& anchor);
    void DestroyCarryVisuals();
    void ClearCarryData();
    // 遷移用の鎖を1フレーム進めて宝石の表示を合わせる（map=nullptr で地形を無視）
    void StepCarry(float dt, MapChip2D* map);
    void SetIris(const Vector3& center, float radius);
    void SetIrisToGem(float radius);
    void SetBackdropAlpha(float alpha);
    void BeginDescend();
    void BeginIrisClose();
    void Finish();
    Vector3 GetGemPosition() const;

    // --- シネマティック ---
    Object3D* GetActor() const;
    Vector3 GetActorHand(Object3D* actor) const;
    // 飛び去った後の本人を隠す（通常更新が毎フレーム姿勢を戻すので、暗転中は毎フレーム呼ぶ）
    void HideActor();
    void FindGoalPosition(MapChip2D* map, const Vector3& playerPos);
    void DriveCamera(const Vector3& target, float zoom);
    void RestoreCamera();
    void BeginFlyBy(Object3D* actor);
    void StepFly(float dt, Object3D* actor);
    void DestroyFly();

    Params params_;
    Phase phase_ = Phase::kNone;
    float phaseTime_ = 0.0f;
    bool coveredEvent_ = false;

    ChainManager* chains_ = nullptr; // 現在のシーンの鎖管理（非所有。Covered で切り離す）
    MapChip2D* map_ = nullptr;       // 現在のシーンのマップ（非所有）
    Player2D* player_ = nullptr;     // 現在のシーンのプレイヤー（非所有。Covered で切り離す）
    GameCamera* camera_ = nullptr;   // ゲームカメラ（非所有。アプリが持つのでシーンをまたいで有効）

    // 遷移側が所有する宝石と鎖
    std::unique_ptr<Chain2D> carryChain_;
    std::unique_ptr<Treasure2D> carryTreasure_;
    int carryUnits_ = 0;
    bool hasCarryData_ = false;
    ChainParams carryParams_;
    EndWeight carryWeight_;

    // 飛び抜ける時の宝石と鎖（大きさを合わせた別物）
    std::unique_ptr<Chain2D> flyChain_;
    std::unique_ptr<Treasure2D> flyTreasure_;

    // 黒板（穴あき）と黒い背景板
    std::unique_ptr<Object3D> iris_;
    std::unique_ptr<Object3D> backdrop_;
    Vector3 irisCenter_ = { 0.0f, 0.0f, 0.0f };
    float irisRadius_ = 0.0f;
    float backdropAlpha_ = 0.0f;
    float coverRadius_ = 40.0f;   // 画面を確実に覆う穴の半径（画面幅 + 高さ）

    // カメラ
    bool cameraControlled_ = false;
    Vector3 cameraStart_ = { 0.0f, 0.0f, -10.0f };
    Vector3 cameraCurrent_ = { 0.0f, 0.0f, -10.0f };
    float baseOrthoW_ = 20.0f;
    float baseOrthoH_ = 11.25f;
    float currentZoom_ = 1.0f;

    // 演技
    Vector3 goalPos_ = { 0.0f, 0.0f, 0.0f };
    Vector3 actorBasePos_ = { 0.0f, 0.0f, 0.0f }; // 演技開始時のモデル座標（足元）
    float actorBaseYaw_ = 0.0f;
    Vector3 enterStart_ = { 0.0f, 0.0f, 0.0f };
    float flyStartX_ = 0.0f;
    float flyEndX_ = 0.0f;

    // 降下（スポットライト）
    float screenBottomY_ = 0.0f;
    float descendStartY_ = 0.0f;
    float descendDist_ = 0.0f;
    float descendDuration_ = 0.8f;

    // 着地（次シーン）
    Vector3 landStart_ = { 0.0f, 0.0f, 0.0f };
    Vector3 landFallback_ = { 0.0f, 0.0f, 0.0f }; // ソケットがまだ計算されていない間の着地目標
    float landDuration_ = 0.8f;

};
