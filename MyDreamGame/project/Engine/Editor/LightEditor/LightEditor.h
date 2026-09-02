#pragma once
#include <Windows.h>
#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include "Core/Utility/Structs.h"
#include "Core/Utility/Vector3.h"
#include "Core/Utility/Vector4.h"

class ModelCommon;

struct SpotLightItem {
    std::string name = "SpotLight";
    bool enabled = true;
    Vector4 color = {1.0f, 1.0f, 1.0f, 1.0f};
    Vector3 position = {0.0f, 2.0f, 0.0f};
    Vector3 direction = {0.0f, -1.0f, 0.0f};
    float intensity = 4.0f;
    float distance = 10.0f;
    float decay = 2.0f;
    float angleDeg = 30.0f;
    float falloffDeg = 20.0f;

    // 危険度・当たり判定設定
    bool isDangerous = true;        //!< プレイヤーへの当たり判定（当たると即死）

    // 追従・アニメーション設定
    LightFollowType followType = LightFollowType::None;
    Vector3 followOffset = {0.0f, 0.0f, 0.0f};
    Vector3 rotateAxis = {0.0f, 1.0f, 0.0f};
    float rotateSpeed = 45.0f;      // 度/秒
    float currentAnimAngle = 0.0f;  // 現在の回転累積角
    Vector3 baseDirection = {0.0f, -1.0f, 0.0f}; // 自動回転の基準向き
};

class LightEditor {
public:
    LightEditor();
    ~LightEditor() = default;

    void Initialize(ModelCommon* modelCommon);

    // 毎フレームのライト追従・アニメーション更新およびGPUバッファへの同期
    void Update(float deltaTime, ModelCommon* modelCommon, const Vector3* playerPos = nullptr);

    // プレイヤー（点・半径またはAABB）が危険なスポットライトの光に当たっているかチェック
    bool CheckPlayerHit(const Vector3& playerPos, float playerRadius = 0.4f) const;
    bool CheckAABBHit(const AABB2D& aabb) const;

#ifdef USE_IMGUI
    // メインタブ画面描画（ゲームビューやリプレイエディターと同じメイン領域に配置：画面プレビューのみ）
    void DrawViewport(D3D12_GPU_DESCRIPTOR_HANDLE renderTextureSrvHandle, const Matrix4x4* viewProjMatrix = nullptr, bool* pOpen = nullptr);
    void DrawViewportContent(D3D12_GPU_DESCRIPTOR_HANDLE renderTextureSrvHandle, const Matrix4x4* viewProjMatrix = nullptr);

    // 画面オーバーレイ描画（スポットライトのコーン、Z=0平面の危険円、プレイヤーの当たり判定等）
    void DrawOverlay(const Matrix4x4& viewProjMatrix, ImVec2 viewportPos, ImVec2 viewportSize, const AABB2D* playerAABB = nullptr);

    // 下部タブ画面描画（ログやドープシートと同じ位置に配置：「スポットライト」設定パネル）
    void DrawBottomPanel(ModelCommon* modelCommon, bool* pOpen = nullptr);

    // 設定UI描画（DrawBottomPanelやインスペクターから利用）
    void DrawLightEditorUI(ModelCommon* modelCommon);
#endif

    // スポットライト操作
    void AddSpotLight();
    void RemoveSpotLight(int index);
    void DuplicateSpotLight(int index);

    // 設定の保存・読み込み
    void SaveLightingConfig(ModelCommon* modelCommon);
    void LoadLightingConfig(ModelCommon* modelCommon);

    // ウィンドウ表示制御
    bool IsVisible() const { return isVisible_; }
    void SetVisible(bool visible) { isVisible_ = visible; }
    bool IsHovered() const { return isHovered_; }

    // 当たり判定デバッグ表示
    bool IsShowDebugCollision() const { return showDebugCollision_; }
    void SetShowDebugCollision(bool show) { showDebugCollision_ = show; }

    // 環境光・グローバルライトのゲッター/セッター
    float GetAmbientIntensity() const { return ambientIntensity_; }
    void SetAmbientIntensity(float intensity) { ambientIntensity_ = intensity; }

    const std::vector<SpotLightItem>& GetSpotLights() const { return spotLights_; }
    std::vector<SpotLightItem>& GetSpotLights() { return spotLights_; }

private:
    bool isVisible_ = true;
    bool isHovered_ = false;
    bool showDebugCollision_ = true; //!< 当たり判定とライトコーンのデバッグ可視化
    int selectedLightIndex_ = 0;

    // 環境光（暗闇調整用：0.0fで完全な暗闇）
    float ambientIntensity_ = 1.0f;

    // 平行光源 & 点光源
    bool enableDirectional_ = true;
    Vector4 directionalColor_ = {1.0f, 1.0f, 1.0f, 1.0f};
    Vector3 directionalDir_ = {-0.038f, -0.962f, 0.268f};
    float directionalIntensity_ = 1.0f;
    bool enableFlatShading_ = false;

    bool enablePoint_ = false;
    Vector4 pointColor_ = {1.0f, 1.0f, 1.0f, 1.0f};
    Vector3 pointPos_ = {0.0f, 2.0f, 0.0f};
    float pointIntensity_ = 1.0f;
    float pointRadius_ = 10.0f;
    float pointDecay_ = 1.0f;

    // スポットライト群
    std::vector<SpotLightItem> spotLights_;
};
