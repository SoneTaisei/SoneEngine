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
#include "Graphics/Skybox.h"
#include "Graphics/DebugCamera.h"
#include "Resource/Primitive/PrimitiveManager.h"
#include "GameObject/PrimitiveObject.h"
#include <vector>
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

    std::vector<std::shared_ptr<GameObject>> searchlightObjects_;

    float titleTimer_ = 0.0f;
    // --- フェーズ管理 ---
    enum class Phase {
        kTitle,              // タイトル画面
        kTransitionToSelect, // ステージ選択へのカメラ移動演出中
        kStageSelect,        // ステージ選択画面
        kTransitionToGame,   // ゲーム遷移中
    };

    Phase phase_ = Phase::kTitle;

    // ステージ選択時の目標カメラ座標・角度（画像で指定された数値）
    Vector3 targetSelectPos_ = { -18.58f, 53.63f, -43.40f };
    Vector3 targetSelectRot_ = { 0.785398f, 0.383972f, 0.0f }; // 45.0°, 22.0°, 0.0°

    // カメラ移動補間用
    Vector3 transitionStartPos_{};
    Vector3 transitionStartRot_{};
    float transitionTimer_ = 0.0f;
    float transitionDuration_ = 1.8f; // カメラ全体の移動時間（秒）
    float logoFadeDuration_ = 0.6f;   // ロゴとライトのフェードアウト時間（秒）
    float titleLogoAlpha_ = 1.0f;
    float searchlightAlpha_ = 1.0f;
    bool enableCinematicSway_ = false; // カメラ調整中は固定できるようにする

    // --- ステージ選択インタラクション ---
    int selectedStageIndex_ = 0; // 0: select_1, 1: select_2, 2: select_3
    Vector4 selectHighlightColor_ = { 1.0f, 0.88f, 0.2f, 1.0f }; // 選択中のハイライト色 (ゴールド/黄色)
    Vector4 unselectedColor_ = { 1.0f, 0.0f, 0.0f, 1.0f };       // 非選択の色 (赤色: ステージの存在が分かるようにする)
    float stageSelectPulseTimer_ = 0.0f;
    bool enableStageSelectPulse_ = true;

    void UpdateStageSelectInteraction(float dt);

    // --- ゲームシーン移行演出 (アイリスアウト: 選択オブジェクトに向かって円が閉じる) ---
    float gameTransitionTimer_ = 0.0f;
    float gameTransitionDuration_ = 0.85f; // 暗転完了までの時間 (秒)
    float irisMaxRadius_ = 3.2f;
    Vector2 irisCenterUV_ = { 0.5f, 0.5f };
    bool isIrisOutActive_ = false;

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

    // --- エディター停止中用 ---
    void UpdateEditor() override;

    bool isFirstFrame_ = true;
};
