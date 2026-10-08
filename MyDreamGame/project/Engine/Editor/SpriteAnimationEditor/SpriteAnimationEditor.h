#pragma once
#include <string>
#include <vector>
#include "Core/Utility/Structs.h"
#include "GameObject/WorldGif.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

class SpriteAnimationEditor {
public:
    enum class EditorMode {
        World3D, // 3Dワールド配置（板ポリ）モード
        Sprite2D // 2Dスプライト（UI）モード
    };

    SpriteAnimationEditor();
    ~SpriteAnimationEditor() = default;

    void Initialize();
    void Update(float deltaTime);

#ifdef USE_IMGUI
    void DrawUI(bool* pOpen, const ImVec2& gameViewPos = ImVec2(0.0f, 0.0f), const ImVec2& gameViewSize = ImVec2(0.0f, 0.0f));
#endif

    // ディレクトリ内のファイルリストを再読み込み
    void RefreshFileList();
    void RefreshStageList();

    // 2Dスプライト用JSON設定
    bool SaveConfigToJson(const std::string& jsonPath);
    bool LoadConfigFromJson(const std::string& jsonPath);

private:
    std::string GetDefaultJsonPath(const std::string& imagePath) const;
    void ApplyDefaultFrameSize();
    void OnFileSelected(int index);

#ifdef USE_IMGUI
    void DrawWorldPlacementTab(const ImVec2& gameViewPos, const ImVec2& gameViewSize);
    void Draw2DSpriteTab(const ImVec2& gameViewPos, const ImVec2& gameViewSize);
    void DrawTransformGizmo(const ImVec2& vpPos, const ImVec2& vpSize, WorldGifObject* selectedObj);
#endif

private:
    const std::string targetDirectory_ = "resources/Sprite/Original/gif";

    // モード切り替え
    EditorMode currentMode_ = EditorMode::World3D;

    // ファイル一覧
    std::vector<std::string> fileList_;
    int selectedFileIndex_ = -1;
    uint32_t loadedTextureHandle_ = 0;
    Vector2 textureBaseSize_ = { 0.0f, 0.0f };

    // グリッド分割パラメータ
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

    // 2Dスプライト配置用パラメータ
    Vector2 placementPos_ = { 100.0f, 100.0f };
    Vector2 placementSize_ = { 64.0f, 64.0f };
    Vector4 placementColor_ = { 1.0f, 1.0f, 1.0f, 1.0f };
    bool showInGameOverlay_ = false;

    // 2D用JSON入出力用バッファ・状態
    char customJsonPath_[260] = "";
    std::string statusMessage_ = "";
    float statusTimer_ = 0.0f;
    int codeTabSelection_ = 0;

    // ------------------------------------------------------------------------
    // 3Dワールド配置（板ポリ）用メンバー
    // ------------------------------------------------------------------------
    std::vector<std::string> stageList_;
    int selectedStageIndex_ = 0;
    int selectedPlacedGifIndex_ = -1;

    // 矢印ギズモ
    bool showTransformGizmo_ = true;
    bool isDraggingGizmo_ = false;
    int gizmoActiveAxis_ = -1; // 0: X, 1: Y, 2: Z
    bool isSnapEnabled_ = false;
    float snapStep_ = 0.5f;
    Vector3 gizmoDragStartPos_ = { 0.0f, 0.0f, 0.0f };
    Vector2 gizmoDragStartMouse_ = { 0.0f, 0.0f };
    Vector2 gizmoDragAxisDir2D_ = { 0.0f, 0.0f };
    float gizmoDragAxisLength2D_ = 1.0f;

    // シーン連動
    bool autoSyncWithScene_ = true;
    void SyncWithActiveScene();
};
