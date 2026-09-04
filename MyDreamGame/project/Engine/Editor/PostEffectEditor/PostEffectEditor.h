#pragma once
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include "Renderer/DirectXCommon/DirectXCommon.h"

#ifdef USE_IMGUI
#include "imgui.h"
#endif

// ポストエフェクトで選択可能なシェーダータイプ
enum class PostEffectShaderType {
    Grayscale,
    Sepia,
    Vignette,
    Blur,
    RadialBlur,
    Dissolve,
    Noise,
    Composite,
    Count
};

// 1つのポストエフェクト設定単位
struct PostEffectItem {
    std::string name = "PostEffect";
    bool enabled = true; // このポストエフェクト全体の有効化
    PostEffectShaderType shaderType = PostEffectShaderType::Sepia; // 選択されたシェーダー

    // 複合エフェクト用パラメータ
    DirectXCommon::CompositeParams params{};

    PostEffectItem(const std::string& n = "PostEffect", PostEffectShaderType type = PostEffectShaderType::Sepia);
};

class PostEffectEditor {
public:
    PostEffectEditor();
    ~PostEffectEditor() = default;

    void Initialize();
    void Update(float deltaTime);

    // DirectXCommonに現在選択中のポストエフェクトパラメータを適用
    void ApplyToDirectXCommon();

#ifdef USE_IMGUI
    // ヒエラルキーウィンドウ内にポストエフェクト一覧を描画
    void DrawHierarchy();

    // 「ポストエフェクト」ウィンドウ内に編集UIを描画
    void DrawUI(bool* pOpen = nullptr);
#endif

    // ポストエフェクトの追加・複製・削除
    void AddPostEffect(const std::string& name = "");
    void DuplicatePostEffect(int index);
    void RemovePostEffect(int index);
    void ClearAll();

    // ファイル保存・読込・走査
    bool SaveToFile(const std::string& filePath = "");
    bool LoadFromFile(const std::string& filePath = "");
    bool DeleteFile(const std::string& filePath = "");
    void ScanPostEffectFiles();

    std::string ResolveFilePath(const std::string& fileNameOrPath) const;
    const std::string& GetCurrentFilePath() const { return currentFilePath_; }
    void SetCurrentFilePath(const std::string& path);
    std::string GetCurrentFileName() const;
    static std::string StripJsonExtension(const std::string& filename);

    // アクセサ
    int GetSelectedPostEffectIndex() const { return selectedPostEffectIndex_; }
    void SetSelectedPostEffectIndex(int idx);
    PostEffectItem* GetSelectedPostEffect();
    const PostEffectItem* GetSelectedPostEffect() const;

    const std::vector<PostEffectItem>& GetPostEffects() const { return postEffects_; }
    std::vector<PostEffectItem>& GetPostEffects() { return postEffects_; }

    const std::string& GetStatusMessage() const { return statusMessage_; }
    void SetStatusMessage(const std::string& msg, float duration = 3.0f) {
        statusMessage_ = msg;
        statusMessageTimer_ = duration;
    }

    bool IsShouldFocusWindow() const { return shouldFocusWindow_; }
    void SetShouldFocusWindow(bool f) { shouldFocusWindow_ = f; }
    void SetOnSelectCallback(std::function<void()> cb) { onSelectCallback_ = cb; }
    void SetOnFileChangedCallback(std::function<void()> cb) { onFileChangedCallback_ = cb; }

private:
    bool shouldFocusWindow_ = false;
    std::function<void()> onSelectCallback_;
    std::function<void()> onFileChangedCallback_;
    std::vector<PostEffectItem> postEffects_;
    int selectedPostEffectIndex_ = 0;

    // ファイル管理
    std::string currentFilePath_ = "resources/json/shared/PostEffect/post_effect_config.json";
    std::vector<std::string> availableFiles_;
    char saveFileNameBuf_[128] = "post_effect_config.json";
    int selectedFileComboIdx_ = -1;

    std::string statusMessage_ = "";
    float statusMessageTimer_ = 0.0f;
};
