#pragma once
#include "Renderer/DirectXCommon/DirectXCommon.h"
#include "Effect/ParticleCommon.h"
#include "Effect/ParticleManager.h"
#include "Scene/IScene.h"
#include "Core/Utility/TransformFunctions.h"
#include <d3d12.h>
#include <memory>
#include <vector>

// 2Dゲーム用クラス
#include "Game2D/Player/Player2D.h"
#include "Game2D/MapChip2D.h"
#include "Game2D/Chain/ChainManager.h"
#include "Game2D/Chain/PlayerChainPostEffect.h"
#include "Game2D/Security/AlertSystem.h"
#include "Effect/TutorialPosterSet.h"

class GameCamera;
struct ImVec2;

#include "GameObject/PrimitiveObject.h"
#include "Resource/Primitive/PrimitiveManager.h"
#include "Resource/Primitive/PrimitiveCone.h"
class Skybox;


enum class GameState {
    StartReady,
    Playing,
    Clear,
    Captured   // 警戒度が満タン → 捕獲演出 → ステージ選択へ
};

class GameScene : public IScene {
public:
    static std::string s_TargetMapFilePath;
    static bool s_QuickRestart; // ミス直後の読み直し：Ready の待ちを短くする
    // 作り直した後も中間ポイントの記録を残すか。
    // 同じ回の続き（ポーズの「セーブポイント」）だけ true。新しく始めた回は false のままで記録を捨てる
    static bool s_ResumeFromSavePoint;

    ~GameScene() override;

    void Initialize() override;
    void OnEnter(SceneManager *sceneManager) override;
    void OnExit(SceneManager *sceneManager) override;
    void Update(SceneManager *sceneManager) override;
    void Draw(const Matrix4x4 &viewProjectionMatrix) override;
    void Draw2D() override;
    void RenderShadowPass();
    void DisplayImGui(PrimitiveObject* selectedPrimitive = nullptr) override;
    void DrawEditorOverlay(const Matrix4x4 &viewProjectionMatrix) override;
    void UpdateEditor() override;

    // ヒエラルキー用
    std::vector<Object3D *> GetObjects() override;
    std::vector<ParticleManager *> GetParticles() override;
    std::vector<PrimitiveObject *> GetPrimitives() override;

    // マップチップの取得
    MapChip2D* GetMapChip() override { return map_.get(); }

    // プレイヤーの取得
    Player2D* GetPlayer() override { return player_; }

    // プレイヤー座標を基準とするアイリスイン（フェードイン）・アイリスアウト演出
    void StartIrisIn(const Vector3& playerPos, float duration = 1.2f);
    void UpdateIrisIn(const Vector3& playerPos, float dt);
    void StartIrisOut(const Vector3& worldPos, float duration = 0.5f);
    void StartIrisOutUV(const Vector2& centerUV, float duration = 0.7f);
    void UpdateIrisOut(float dt);
    Vector2 WorldToScreenUV(const Vector3& worldPos) const;
    bool IsIrisInActive() const { return isIrisInActive_; }
    bool IsIrisOutActive() const { return isIrisOutActive_; }

private:
    // カメラ用行列（Updateで必要なためメンバに追加）
    Matrix4x4 viewProjection_ = TransformFunctions::MakeIdentity4x4();
    Matrix4x4 cameraMatrix_ = TransformFunctions::MakeIdentity4x4();

    // ---------------------------------------------------
    // 2Dゲーム用オブジェクト
    std::unique_ptr<GameObject> playerObj_;
    Player2D* player_ = nullptr;
    std::unique_ptr<MapChip2D> map_;

    // 鎖の管理（プレイヤー鎖 + 吊り鎖 + 落とした自由鎖、ユニット制）
    std::unique_ptr<ChainManager> chainManager_;

    // 警戒度（0〜100。満タンで捕獲）
    std::unique_ptr<AlertSystem> alert_;
    void DrawCaptureOverlay(const ImVec2& viewPos, float viewWidth, float viewHeight);
    
    // 状態追跡用フラグ（Update内のstatic変数をメンバ化）
    bool wasCurrentlyPlaying_ = false;
    bool wasPlayingLastFrame_ = false;
    bool wasRewindingLastFrame_ = false;
    bool playerWasDead_ = false;      // 復活の検出（警戒度の猶予用）
    bool capturedByMiss_ = false;     // 捕獲画面の原因（true = 接触・落下などのミス、false = 見つかった回数）
    
    std::unique_ptr<Skybox> skybox_; // Skyboxのインスタンス
    uint32_t skyboxTextureHandle_ = 0;

    // マップ背景用板ポリゴン（スポットライト等のライティング視認用）
    std::unique_ptr<PrimitiveObject> backgroundPlane_;

    // 操作説明の映像（ポスター）：マップごとの JSON に保存。無ければ木の板の上に振り子の説明を 1 枚置く
    std::unique_ptr<TutorialPosterSet> tutorialPosters_;
    void SetupTutorialPoster();
    /// <summary>今遊んでいるマップのパス（実際に読んだファイル。エディタの一時ファイルの時はエディタで選んでいるファイル名）。やり直しと映像の保存先に使う</summary>
    std::string ResolveCurrentMapPath() const;
public:
    // マップと編集中のファイル名から、記録の鍵に使う「本当のステージのパス」を決める。
    // セーブポイント側も同じ関数を使うことで、書く時と読む時で鍵が食い違わないようにする
    static std::string ResolveStagePath(const class MapChip2D* map);
private:

    // ---------------------------------------------------
    // 共通システム
    // ---------------------------------------------------
    // コマンドリストを覚えておくための変数
    


    // ステージクリア遷移で覆い切った後の行き先（いったんステージ選択へ戻る）
    void GoToNextStage(SceneManager* sceneManager);

    // 灰色の背景壁（BackgroundPlane）の設定保存・読込
    void SaveBackgroundConfig();
    void LoadBackgroundConfig();

    // 警備員の懐中電灯スポットライトをModelCommonに同期
    void UpdateGuardLights();

    // 2Dゲーム用カメラ（正射影＋プレイヤー追従）をこのシーンのものへ張り直す。
    // OnExit() で解除した状態を復帰時に戻すため、Initialize / OnEnter から呼ぶ。
    void SetupGameCamera();

    // 毎フレームの保険。他エディター等で正射影が解除されたままだと追従処理が走らないため戻す
    void EnsureGameCameraMode();

    GameState gameState_ = GameState::StartReady;
    float stateTimer_ = 0.0f;
    float transitionAlpha_ = 0.0f; // 画面遷移演出用(フェードイン - アイリスインへ置き換え)

    // アイリスイン演出用
    bool isIrisInActive_ = false;
    float irisInTimer_ = 0.0f;
    float irisInDuration_ = 1.2f;
    float irisInMaxRadius_ = 3.2f; // 約2倍に拡大（画面全体を十分に覆う）

    // アイリスアウト演出用
    bool isIrisOutActive_ = false;
    float irisOutTimer_ = 0.0f;
    float irisOutDuration_ = 0.5f;
    Vector3 irisOutTargetPos_ = { 0.0f, 0.0f, 0.0f };
    Vector2 irisOutCenterUV_ = { 0.5f, 0.5f };

    // ---------------------------------------------------
    // 死亡演出（帽子・鎖・宝石の残留、アイリスアウト/インリスポーン）
    // ---------------------------------------------------
    std::shared_ptr<GameObject> deathHatObject_;
    bool isDeathHatActive_ = false;
    Vector3 deathHatPos_ = { 0.0f, 0.0f, 0.0f };
    Vector3 deathHatVelocity_ = { 0.0f, 0.0f, 0.0f };
    float deathHatFloorY_ = 0.0f;
    float deathHatRotationZ_ = 0.0f;
    bool isDeathHatGrounded_ = false;
    float deathHatGroundedTimer_ = 0.0f;
    bool isIrisOutStarted_ = false;

    bool isDeathSequenceActive_ = false;
    float deathSequenceTimer_ = 0.0f;
    Vector3 deathRespawnPos_ = { 0.0f, 0.0f, 0.0f };

    void TriggerDeathSequence();
    void UpdateDeathSequence(float dt, SceneManager* sceneManager);

    // ---------------------------------------------------
    // クリア演出（交差スポットライト・煙幕脱出・アイリスアウト）
    // ---------------------------------------------------
    std::unique_ptr<class PrimitiveCone> spotBeamCone_;
    std::unique_ptr<PrimitiveObject> spotBeamObj1_;
    std::unique_ptr<PrimitiveObject> spotBeamObj2_;

    bool isClearSequenceActive_ = false;
    bool isClearSequenceFinished_ = false; // クリア演出が完全に終わり、暗転した後にtrueになる
    float clearSequenceTimer_ = 0.0f;
    Vector3 clearTargetPos_ = { 0.0f, 0.0f, 0.0f };
    float clearTargetTopY_ = 0.0f;
    bool isClearSmokeSpawned_ = false;
    bool isClearEscaped_ = false;
    bool isClearIrisStarted_ = false;
    float clearCameraStartScale_ = 1.0f;
    Vector3 clearCameraStartPos_ = { 0.0f, 0.0f, -10.0f };
    bool isClearExitIrisActive_ = false; // クリア画面でSPACEを押した後のタイトル遷移用アイリスアウト

    void TriggerClearSequence(const Vector3& goalPos, float goalTopY);
    void UpdateClearSequence(float dt, SceneManager* sceneManager);
    void UpdateSpotBeams(const Vector3& targetPos, float targetTopY, float progress1, float progress2, float alpha1, float alpha2);
    void DrawClearSpotlightBeams();

    // スペース長押し（鎖エイム）時ポストエフェクト (Player_Chain.json)
    std::unique_ptr<PlayerChainPostEffect> playerChainPostEffect_;
    float spaceHoldTimer_ = 0.0f;


    // ---------------------------------------------------
    // ポーズメニュー関連
    // ---------------------------------------------------
    bool isPaused_ = false;
    int pauseMenuIndex_ = 0; // 0: リトライ (restartText.png), 1: タイトル (titleText.png)
    float pausePulseTimer_ = 0.0f;
    float pauseCooldown_ = 0.0f;

    std::unique_ptr<class Sprite> pauseBackdropSprite_;
    std::unique_ptr<class Sprite> pauseTitleSprite_;
    std::unique_ptr<class Sprite> pauseRestartSprite_;
    std::unique_ptr<class Sprite> pauseTitleTextSprite_;
    std::unique_ptr<class Sprite> pauseSaveSprite_; // 「セーブポイント」。記録がある時だけ出す

    uint32_t pauseBackdropTexHandle_ = 0;
    uint32_t pauseTitleTexHandle_ = 0;
    uint32_t pauseRestartTexHandle_ = 0;
    uint32_t pauseSaveTexHandle_ = 0;
    // ポーズの項目に「セーブポイント」を出すか（記録があるか）。更新と描画で同じ判断を使う
    bool HasSavePointItem() const;
    uint32_t pauseTitleTextTexHandle_ = 0;

    // 収集アイテム（小さい宝石）の HUD。ImGui ではなくゲーム画面にスプライトで描く（左下のひし形と数字、クリア画面の「宝石 N / M」）
    std::vector<std::unique_ptr<class Sprite>> gemIconSprites_;     // ひし形（塗り）
    std::vector<std::unique_ptr<class Sprite>> gemOutlineSprites_;  // ひし形（枠だけ）
    std::vector<std::unique_ptr<class Sprite>> gemDigitSprites_;    // 数字と「/」（gem_digits.png の帯から切り抜く）
    std::unique_ptr<class Sprite> gemLabelSprite_;                  // 「宝石」（クリア画面）
    std::unique_ptr<class Sprite> gemCompleteSprite_;               // 「コンプリート!」（クリア画面）
    uint32_t gemIconTexHandle_ = 0;
    uint32_t gemOutlineTexHandle_ = 0;
    uint32_t gemDigitsTexHandle_ = 0;
    uint32_t gemLabelTexHandle_ = 0;
    uint32_t gemCompleteTexHandle_ = 0;
    // 目のアイコン（残り回数）、発見直後の画面の縁の赤、警備員の頭上の「！」「？」と見られているゲージ。ImGui ではなくスプライト
    std::vector<std::unique_ptr<class Sprite>> eyeOpenSprites_;
    std::vector<std::unique_ptr<class Sprite>> eyeSpentSprites_;
    std::vector<std::unique_ptr<class Sprite>> edgeGlowSprites_;   // 上下左右の 4 本
    std::vector<std::unique_ptr<class Sprite>> markExclaimSprites_;
    std::vector<std::unique_ptr<class Sprite>> markQuestionSprites_;
    std::vector<std::unique_ptr<class Sprite>> markBarBackSprites_;
    std::vector<std::unique_ptr<class Sprite>> markBarFillSprites_;
    uint32_t eyeOpenTexHandle_ = 0;
    uint32_t eyeSpentTexHandle_ = 0;
    uint32_t markExclaimTexHandle_ = 0;
    uint32_t markQuestionTexHandle_ = 0;
    // 警戒度のバーと文字。ImGui だとエディタのゲームビューにしか出ず、製品版では消えるのでスプライトで描く
    std::vector<std::unique_ptr<class Sprite>> alertBarSprites_; // 下地・塗り・枠の 4 本
    std::unique_ptr<class Sprite> alertLabelSprite_;             // 「警戒度」
    std::unique_ptr<class Sprite> alertSeenSprite_;              // 「見られている」
    std::unique_ptr<class Sprite> alertSuspectSprite_;           // 「怪しまれている」
    // 開始・クリア・ポーズの文字。これも ImGui だと製品版で消えるのでスプライトで描く
    std::unique_ptr<class Sprite> readyTextSprite_;    // 「READY...」
    std::unique_ptr<class Sprite> goTextSprite_;       // 「GO!」
    std::unique_ptr<class Sprite> stageClearSprite_;   // 「STAGE CLEAR!」
    std::unique_ptr<class Sprite> clearPromptSprite_;  // 「Press SPACE / A to Stage Select」
    std::unique_ptr<class Sprite> pauseGuideSprite_;   // ポーズ中の操作ガイド
    std::vector<std::unique_ptr<class Sprite>> rankSprites_;      // 静穏 / 潜入 / 強行 / 騒然
    std::unique_ptr<class Sprite> statSpottedSprite_;  // 「発見」
    std::unique_ptr<class Sprite> statReportSprite_;   // 「通報」
    std::unique_ptr<class Sprite> statNoiseSprite_;    // 「騒音」
    std::unique_ptr<class Sprite> statPeakSprite_;     // 「最大警戒度」
    std::vector<std::unique_ptr<class Sprite>> statTimesSprites_; // 「回」を 3 個
    uint32_t alertLabelTexHandle_ = 0;
    uint32_t alertSeenTexHandle_ = 0;
    uint32_t alertSuspectTexHandle_ = 0;
    float hudTime_ = 0.0f; // 点滅・上下ゆれ用
    // ゲーム画面の HUD をまとめてスプライトで描く（宝石・目・縁の赤・警備員の合図）。3D と粒子の後、ポーズの前
    void DrawHudSprites(const Matrix4x4& viewProjection);
    // ゲームが動いているか（エディタで編集しているだけの間は false）。ゲーム中の表示を出すかの判断に使う
    bool IsGamePlaying() const;
    // 警戒度のバーと文字（右上）。DrawHudSprites から呼ぶ
    void DrawAlertBarSprites();
    // 開始・クリア・ポーズの文字。DrawHudSprites から呼ぶ
    void DrawStageStateSprites();
    // クリア画面の内訳「発見 N 回 …」を中央に並べる
    void DrawClearStatLine(const struct AlertRank& rank, float centerY);
    // 数字列（"0123456789/" と空白）を x, y から並べて描く。戻り値は描いた幅
    // startIndex: 使うスプライトの先頭番号（同じフレームで二か所に描く時は別の番号から使う。スプライトは描画バッファを 1 つしか持たない）
    float DrawGemDigits(const char* text, float x, float y, float cellW, float cellH, const Vector4& color, size_t startIndex = 0);

    void UpdatePauseMenu(float dt, SceneManager* sceneManager);
    void DrawPauseMenu();
};