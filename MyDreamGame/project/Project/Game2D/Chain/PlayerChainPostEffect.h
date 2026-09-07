#pragma once
#include <string>
#include <vector>
#include <memory>
#include <filesystem>
#include "Renderer/DirectXCommon/DirectXCommon.h"
#include "Editor/PostEffectEditor/PostEffectEditor.h"

#ifdef USE_IMGUI
#include "imgui.h"
#endif

/// <summary>
/// プレイヤーのスペース長押し時（鎖のエイム中など）に Player_Chain.json のポストエフェクトを適用するクラス
/// </summary>
class PlayerChainPostEffect {
public:
    PlayerChainPostEffect() = default;
    ~PlayerChainPostEffect() = default;

    /// <summary>初期化（JSONのロード）</summary>
    void Initialize(const std::string& jsonPath = "resources/json/shared/PostEffect/Player_Chain.json");

    /// <summary>毎フレーム更新（トリガー判定に応じたアルファ補間、ホットリロード監視）</summary>
    void Update(float dt, bool isTriggered);

    /// <summary>DirectXCommonにパラメータを合成適用</summary>
    void ApplyToDirectXCommon(DirectXCommon* dxCommon);

    /// <summary>即時リセット（巻き戻し・シーン遷移・死亡時など）</summary>
    void Reset(DirectXCommon* dxCommon = nullptr);

#ifdef USE_IMGUI
    /// <summary>デバッグUI描画</summary>
    void DisplayImGui();
#endif

    /// <summary>JSONファイルから設定を読み込み</summary>
    bool LoadFromJson(const std::string& jsonPath);

    float GetCurrentAlpha() const { return currentAlpha_; }
    bool IsActive() const { return currentAlpha_ > 0.001f; }

    void SetFadeInDuration(float duration) { fadeInDuration_ = duration; }
    void SetFadeOutDuration(float duration) { fadeOutDuration_ = duration; }

private:
    std::string filePath_ = "resources/json/shared/PostEffect/Player_Chain.json";
    std::vector<PostEffectItem> postEffects_;
    std::filesystem::file_time_type lastFileWriteTime_{};

    float currentAlpha_ = 0.0f;     // 0.0f (未適用) 〜 1.0f (最大適用)
    float fadeInDuration_ = 0.08f;  // フェードイン所要時間 (秒)
    float fadeOutDuration_ = 0.08f; // フェードアウト所要時間 (秒)

    bool wasApplied_ = false;       // 直前フレームでDirectXCommonに適用されていたか
};
