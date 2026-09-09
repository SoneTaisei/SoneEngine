#pragma once
#include "Scene/IScene.h"
#include <d3d12.h>
#include "Resource/Model/Model.h"
#include "Resource/Sprite/Sprite.h"
#include "Core/Utility/Utilityfunctions.h"
#include "Effect/ParticleManager.h"
#include <memory>
#include "Effect/ParticleCommon.h"
#include "Effect/windowParticle.h"
#include "GameObject/GameObject.h"
#include "Component/MeshRendererComponent.h"
#include "Component/TransformComponent.h"
#include "Graphics/Skybox.h"
#include "Graphics/DebugCamera.h"
#include "Resource/Primitive/PrimitiveManager.h"
#include "GameObject/PrimitiveObject.h"
#include <vector>
#include <array>
#include "Resource/Model/ModelCommon.h"
#include "Resource/Sprite/SpriteCommon.h"
#include "Effect/ParticleCommon.h"

class TitleScene : public IScene {
public:
    ~TitleScene() override;
    void Initialize() override;
    void OnEnter(SceneManager *sceneManager) override;
    void OnExit(SceneManager *sceneManager) override;
    void Update(SceneManager *sceneManager) override;
    void Draw(const Matrix4x4 &viewProjectionMatrix) override;
    void Draw2D() override;
    void DisplayImGui(PrimitiveObject* selectedPrimitive = nullptr) override;

    // ヒエラルキー用
    std::vector<Object3D *> GetObjects() override;
    std::vector<ParticleManager *> GetParticles() override;
    std::vector<PrimitiveObject *> GetPrimitives() override;

    // 3Dモデル配置JSONファイルパス (タイトル専用: title_obj.json)
    std::string GetLevelDataJsonPath() const override {
        return "resources/json/shared/LevelData/title_obj.json";
    }

private:
    // メンバ変数としてモデル、テクスチャ、座標を持つ
    uint32_t textureHandle_ = 0;
    EulerTransform transform_ = {};
    

    Model *playerModel_ = nullptr;

    std::vector<std::shared_ptr<GameObject>> gameObjects_{};
    std::vector<std::unique_ptr<Sprite>> sprites_{};

    // ■ 追加: パーティクル管理用変数

    // 2. パーティクルリスト (所有権管理用)
    std::vector<std::unique_ptr<ParticleManager>> particles_{};

    // 3. 個別のパーティクル操作用ポインタ (Emit呼び出し用)
    windowParticle *windowParticle_ = nullptr;

    // 4. エミッタ (発生設定)
    Emitter windowEmitter_{};

    // 5. SRVインデックス (他と被らない番号)
    const int srvIndex_ = 110;

    // ■ 追加: タイトルシーン専用カメラ
    EulerTransform cameraTransform_{}; // カメラの座標・回転
    Matrix4x4 viewProjection_{};  // 描画に使う行列

    std::unique_ptr<Skybox> skybox_; // Skyboxのインスタンス
    uint32_t skyboxTextureHandle_ = 0;

    // --- 怪盗タイトルシーン演出用 ---
    std::unique_ptr<Sprite> titleLogoSprite_;
    uint32_t titleLogoTextureHandle_ = 0;

    // --- タイトルメニュー用（スタート / クレジット） ---
    std::unique_ptr<Sprite> startTextSprite_;
    uint32_t startTextTextureHandle_ = 0;
    Vector2 startTextPos_ = { 515.0f, 430.0f };
    Vector2 startTextSize_ = { 250.0f, 50.0f };

    std::unique_ptr<Sprite> creditTextSprite_;
    uint32_t creditTextTextureHandle_ = 0;
    Vector2 creditTextPos_ = { 512.5f, 520.0f };
    Vector2 creditTextSize_ = { 255.0f, 50.0f };

    // --- タイトルメニュー用（説明書 / ruleBook.png） ---
    std::unique_ptr<Sprite> ruleBookSprite_;
    uint32_t ruleBookTextureHandle_ = 0;
    Vector2 ruleBookPos_ = { 36.0f, 540.0f };
    Vector2 ruleBookSize_ = { 130.0f, 130.0f };
    float ruleBookScale_ = 1.0f;
    float ruleBookBobTimer_ = 0.0f;
    float titlePadCooldown_ = 0.0f;

    // --- ステージ選択の見出し。カメラが着いた後、画面の外から引っ張られるように入ってくる ---
    std::unique_ptr<Sprite> stageSelectTitleSprite_;
    uint32_t stageSelectTitleTextureHandle_ = 0;
    float stageSelectIntroTimer_ = -1.0f;      // 0 未満は「まだ始まっていない」
    float stageSelectIntroDuration_ = 0.55f;   // 入ってくるのにかける時間（秒）
    Vector2 stageSelectTitlePos_ = { 64.0f, 46.0f };   // 落ち着く位置（左上）
    float stageSelectTitleHeight_ = 64.0f;             // 高さ（幅は画像の比率から出す）

    // --- 決定の操作案内（右下）。パッドを触れば A、キーボードを触れば SPACE ---
    std::unique_ptr<Sprite> padPromptSprite_;  // A:決定
    std::unique_ptr<Sprite> keyPromptSprite_;  // SPACE:決定
    uint32_t padPromptTextureHandle_ = 0;
    uint32_t keyPromptTextureHandle_ = 0;
    bool usePadPrompt_ = false;    // 直前に触ったのがパッドか
    float promptHeight_ = 44.0f;   // 案内の高さ（画像の比率から幅を出す）
    float promptMargin_ = 28.0f;   // 画面の端からの余白

    // --- クレジット画面表示用 (credit.png) ---
    std::unique_ptr<Sprite> creditSprite_;
    uint32_t creditTextureHandle_ = 0;
    Vector2 creditPos_ = { 340.0f, 210.0f };
    Vector2 creditSize_ = { 600.0f, 300.0f };
    float creditAlpha_ = 0.0f; // カメラ移動完了で出現、タイトル復帰で非表示
    float creditAnimTimer_ = 0.0f;          // 出現・待機アニメーション用タイマー
    float creditScale_ = 1.0f;              // ポップイン・縮小演出用スケール
    float creditTransitionDuration_ = 1.1f; // クレジットカメラ移動時間 (秒: 少し早く設定)

    int selectedTitleMenu_ = 0; // 0: スタート, 1: クレジット
    float titleMenuPulseTimer_ = 0.0f;
    float titleMenuAlpha_ = 1.0f;

    std::vector<std::shared_ptr<GameObject>> searchlightObjects_;

    float titleTimer_ = 0.0f;
    // --- フェーズ管理 ---
    enum class Phase {
        kTitle,                 // タイトル画面
        kTransitionToSelect,    // ステージ選択へのカメラ移動演出中
        kStageSelect,           // ステージ選択画面
        kTransitionFromSelect,  // ステージ選択からタイトルへのカメラ復帰演出中
        kTransitionToGame,      // ゲーム遷移中
        kTransitionToCredit,    // クレジット画面へのカメラ移動演出中
        kCredit,                // クレジット画面
        kTransitionFromCredit,  // クレジットからタイトルへのカメラ復帰演出中
    };

    Phase phase_ = Phase::kTitle;
    Phase prevPhase_ = Phase::kTitle; // フェーズが変わった瞬間を拾う（見出しの演出開始用）

    // タイトル画面基準カメラ座標・角度
    Vector3 titleCameraPos_ = { 0.0f, 1.2f, -8.5f };
    Vector3 titleCameraRot_ = { 0.06f, 0.0f, 0.0f };

    // ステージ選択時の目標カメラ座標・角度（画像で指定された数値）
    Vector3 targetSelectPos_ = { -18.58f, 53.63f, -43.40f };
    Vector3 targetSelectRot_ = { 0.785398f, 0.383972f, 0.0f }; // 45.0°, 22.0°, 0.0°

    // クレジット表示時の目標カメラ座標・角度（ユーザー指定値）
    // 位置: (2.32, -0.24, -10.43)
    // 角度(ラジアン): (0.015, -1.575, 0.000)
    Vector3 targetCreditPos_ = { 2.32f, -0.24f, -10.43f };
    Vector3 targetCreditRot_ = { 0.015f, -1.575f, 0.0f };

    // カメラ移動補間用
    Vector3 transitionStartPos_{};
    Vector3 transitionStartRot_{};
    float transitionTimer_ = 0.0f;
    float transitionDuration_ = 1.4f; // カメラ全体の移動時間（秒: 1.8fから少し早く調整）
    float logoFadeDuration_ = 0.5f;   // ロゴとライトのフェードアウト時間（秒）
    float titleLogoAlpha_ = 1.0f;
    float searchlightAlpha_ = 1.0f;
    bool enableCinematicSway_ = false; // カメラ調整中は固定できるようにする

    // --- ステージ選択インタラクション ---
    // 0: チュートリアル, 1: select_1, 2: select_2, 3: select_3
    int selectedStageIndex_ = 0;
    Vector4 selectHighlightColor_ = { 1.0f, 0.88f, 0.2f, 1.0f }; // 未クリア選択中のハイライト色 (ゴールド/黄色)
    Vector4 unselectedColor_ = { 1.0f, 0.0f, 0.0f, 1.0f };       // 未クリア非選択の色 (赤色: ステージの存在が分かるようにする)
    Vector4 clearedColor_ = { 0.15f, 0.45f, 1.0f, 1.0f };        // クリア済み・非選択の色 (青色)
    Vector4 clearedHighlightColor_ = { 0.35f, 0.8f, 1.0f, 1.0f }; // クリア済み・選択中のハイライト色 (シアンブルー)
    float stageSelectPulseTimer_ = 0.0f;
    bool enableStageSelectPulse_ = true;

    // --- チュートリアルUI（左下） ---
    std::unique_ptr<Sprite> tutorialUiSprite_;
    uint32_t tutorialUiTextureHandle_ = 0;
    Vector2 tutorialUiPos_ = { 36.0f, 540.0f }; // 左下配置
    Vector2 tutorialUiSize_ = { 130.0f, 130.0f }; // 400x400 の正方形アイコン用サイズ
    float tutorialUiScale_ = 1.0f;
    float tutorialUiAlpha_ = 0.0f;
    float tutorialBobTimer_ = 0.0f;     // 選択時の縦揺れ用タイマー
    float tutorialBobAmplitude_ = 8.0f; // 縦揺れの振幅 (px)
    float tutorialBobFrequency_ = 4.0f; // 縦揺れの周波数 (rad/s)

    void UpdateStageSelectInteraction(float dt);

    // --- ゲームシーン移行演出 (アイリスアウト: 選択オブジェクトに向かって円が閉じる) ---
    float gameTransitionTimer_ = 0.0f;
    float gameTransitionDuration_ = 0.85f; // 暗転完了までの時間 (秒)
    float irisMaxRadius_ = 3.2f;
    Vector2 irisCenterUV_ = { 0.5f, 0.5f };
    bool isIrisOutActive_ = false;

    // --- シーン開始演出 (アイリスイン: 画面中央から円が開いてステージ選択画面が現れる) ---
    float irisInTimer_ = 0.0f;
    float irisInDuration_ = 0.7f;
    Vector2 irisInCenterUV_ = { 0.5f, 0.5f };
    bool isIrisInActive_ = false;

    void StartIrisIn(const Vector2& centerUV = { 0.5f, 0.5f }, float duration = 0.7f);
    void UpdateIrisIn(float dt);
    void StartIrisOut(const Vector2& centerUV, float duration = 0.85f);
    void UpdateIrisOut(float dt, SceneManager* sceneManager);
    Vector2 WorldToScreenUV(const Vector3& worldPos) const;

    // --- 予告状（callingCard）突き刺し演出 ---
    std::shared_ptr<GameObject> callingCardObject_;
    enum class CardThrowPhase {
        kNone,
        kFlying,       // 手前からビルへ高速飛翔
        kStuckWobble,  // 刺さった瞬間の振動・余韻
        kIrisOut,      // 予告状を中心とした暗転
    };
    CardThrowPhase cardPhase_ = CardThrowPhase::kNone;
    Vector3 cardStartPos_{};
    Vector3 cardTargetPos_{};
    Vector3 cardTargetRot_ = { 0.0f, 0.445059f, 0.977384f }; // 0.0°, 25.5°, 56.0°
    Vector3 cardTargetOffset_ = { 1.200f, 7.300f, -1.500f }; // 画像指定の刺さり位置オフセット
    float cardStartScale_ = 0.75f;     // 飛翔開始時のスケール（手前で大きくダイナミックに）
    float cardTargetScale_ = 0.50f;    // 刺さり時のスケール（遠景でも存在感がある約2.5倍サイズ）
    float cardTimer_ = 0.0f;
    float cardFlyDuration_ = 0.36f;     // 飛翔時間 (秒)
    float cardWobbleDuration_ = 0.40f;  // 刺さった後の振動・見せる時間 (秒)
    float cardShakeTimer_ = 0.0f;       // 着弾時の微小カメラ揺れ用タイマー
    Vector3 cameraShakeOffset_{};

    void StartCallingCardThrow(const Vector3& targetPos);
    void UpdateCallingCardThrow(float dt, SceneManager* sceneManager);

    // --- ステージ選択時のガイド看板表示 (plan.obj + stage1.png/stage2.png/stage3.png) ---
    std::shared_ptr<GameObject> stageGuideObject_;
    MeshRendererComponent* stageGuideRenderer_ = nullptr;
    TransformComponent* stageGuideTransform_ = nullptr;
    std::array<uint32_t, 3> stageGuideTextureHandles_{};
    std::array<Vector3, 3> stageGuideOffsets_ = {
        Vector3{ 13.6f, 8.4f, 0.0f },     // ステージ1 (画像1指定値)
        Vector3{ 0.0f, 16.0f, 0.0f },     // ステージ2 (角度調整・位置初期値)
        Vector3{ -18.3f, 15.4f, -4.5f }   // ステージ3 (画像2指定値)
    };
    std::array<Vector3, 3> stageGuideRots_ = {
        Vector3{ 0.0f, 1.256637f, 3.141593f }, // ステージ1 (0.0°, 72.0°, 180.0°)
        Vector3{ 0.0f, 1.239184f, 3.141593f }, // ステージ2 (0.0°, 71.0°, 180.0°)
        Vector3{ 0.0f, 1.221731f, 3.141593f }  // ステージ3 (0.0°, 70.0°, 180.0°)
    };
    Vector3 stageGuideScale_ = { 1.0f, -2.5f, 10.0f }; // 4:1 アスペクト比 (厚み1.0, 高さ-2.5で上下反転解消, 横幅10.0)
    Vector3 currentGuidePos_{};
    Vector3 currentGuideRot_{};
    float currentGuideScaleFactor_ = 0.0f; // ポップイン・縮小アニメーション用 (0.0 ~ 1.0)
    int currentGuideStageIdx_ = -1;
    void UpdateStageGuideBanner(float dt);

    // --- エディター停止中用 ---
    void UpdateEditor() override;

    bool isFirstFrame_ = true;
};
