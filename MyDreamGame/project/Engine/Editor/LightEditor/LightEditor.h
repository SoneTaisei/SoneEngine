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

#ifdef USE_IMGUI
    // メインタブ画面描画（ゲームビューやリプレイエディターと同じメイン領域に配置：画面プレビューのみ）
    void DrawViewport(D3D12_GPU_DESCRIPTOR_HANDLE renderTextureSrvHandle, const Matrix4x4* viewProjMatrix = nullptr, bool* pOpen = nullptr);
    void DrawViewportContent(D3D12_GPU_DESCRIPTOR_HANDLE renderTextureSrvHandle, const Matrix4x4* viewProjMatrix = nullptr);

    // 画面オーバーレイ描画（選択中スポットライトのワイヤーボックス・照射方向等）
    void DrawOverlay(const Matrix4x4& viewProjMatrix, ImVec2 viewportPos, ImVec2 viewportSize);

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

    // 環境光・グローバルライトのゲッター/セッター
    float GetAmbientIntensity() const { return ambientIntensity_; }
    void SetAmbientIntensity(float intensity) { ambientIntensity_ = intensity; }

    const std::vector<SpotLightItem>& GetSpotLights() const { return spotLights_; }
    std::vector<SpotLightItem>& GetSpotLights() { return spotLights_; }

private:
    bool isVisible_ = true;
    bool isHovered_ = false;
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
