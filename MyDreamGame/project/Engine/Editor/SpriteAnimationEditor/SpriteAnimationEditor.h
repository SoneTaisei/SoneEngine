#pragma once
#include <string>
#include <vector>
#include <memory>
#include "Core/Utility/Structs.h"

#ifdef USE_IMGUI
#include "imgui.h"
#endif

class SpriteAnimationEditor {
public:
    SpriteAnimationEditor();
    ~SpriteAnimationEditor() = default;

    void Initialize();
    void Update(float deltaTime);

#ifdef USE_IMGUI
    void DrawUI(bool* pOpen);
#endif

    // ディレクトリ内のファイルリストを再読み込み
    void RefreshFileList();

private:
    const std::string targetDirectory_ = "resources/Sprite/Original/gif";

    // ファイル一覧
    std::vector<std::string> fileList_;
    int selectedFileIndex_ = -1;
    uint32_t loadedTextureHandle_ = 0;
    Vector2 textureBaseSize_ = { 0.0f, 0.0f };

    // パターンA（グリッド分割）パラメータ
    int columns_ = 4;
    int rows_ = 4;
    int totalFrames_ = 16;
    float fps_ = 15.0f;
    bool isLoop_ = true;

    // 再生状態
    bool isPlaying_ = true;
    float animTimer_ = 0.0f;
    int currentFrame_ = 0;

    // プレビュー表示用スケール
    float previewScale_ = 1.0f;
};
