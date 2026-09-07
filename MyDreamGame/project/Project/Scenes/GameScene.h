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
#include "Game2D/Security/AlertSystem.h"
#include "Effect/TutorialPosterSet.h"

class GameCamera;
struct ImVec2;

#include "GameObject/PrimitiveObject.h"
#include "Resource/Primitive/PrimitiveManager.h"
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

    ~GameScene() override;

    void Initialize() override;
    void OnEnter(SceneManager *sceneManager) override;
    void OnExit(SceneManager *sceneManager) override;
    void Update(SceneManager *sceneManager) override;
    void Draw(const Matrix4x4 &viewProjectionMatrix) override;
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

    // プレイヤー座標を基準とするアイリスイン（フェードイン）演出
    void StartIrisIn(const Vector3& playerPos, float duration = 1.2f);
    void UpdateIrisIn(const Vector3& playerPos, float dt);
    Vector2 WorldToScreenUV(const Vector3& worldPos) const;
    bool IsIrisInActive() const { return isIrisInActive_; }

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
    void DrawAlertHud(const ImVec2& viewPos, float viewWidth, float viewHeight);
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
    /// <summary>操作説明の映像の保存先を決めるマップのパス（実際に読んだファイル。エディタの一時ファイルの時はエディタで選んでいるファイル名）</summary>
    std::string ResolvePosterMapPath() const;

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

    uint32_t pauseBackdropTexHandle_ = 0;
    uint32_t pauseTitleTexHandle_ = 0;
    uint32_t pauseRestartTexHandle_ = 0;
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
    float hudTime_ = 0.0f; // 点滅・上下ゆれ用
    // ゲーム画面の HUD をまとめてスプライトで描く（宝石・目・縁の赤・警備員の合図）。3D と粒子の後、ポーズの前
    void DrawHudSprites(const Matrix4x4& viewProjection);
    // 数字列（"0123456789/" と空白）を x, y から並べて描く。戻り値は描いた幅
    // startIndex: 使うスプライトの先頭番号（同じフレームで二か所に描く時は別の番号から使う。スプライトは描画バッファを 1 つしか持たない）
    float DrawGemDigits(const char* text, float x, float y, float cellW, float cellH, const Vector4& color, size_t startIndex = 0);

    void UpdatePauseMenu(float dt, SceneManager* sceneManager);
    void DrawPauseMenu();
};