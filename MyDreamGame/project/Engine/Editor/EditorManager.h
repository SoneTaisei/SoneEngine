#pragma once
#ifdef USE_IMGUI
#include <Windows.h>
#include <d3d12.h>
#include <cstdint>
#include <set>
#include <unordered_map>
#include <memory>
#include <functional>

// UIから操作したいクラスのヘッダーをインクルード
#include "Graphics/DebugCamera.h"
#include "Graphics/GameCamera.h"
#include "Resource/Model/ModelCommon.h"
#include "Scene/SceneFactory.h"
#include "Core/Utility/Structs.h"
#include "Replay/ReplayManager.h"

class SceneManager;
class ParticleManager;
class Object3D;
class PrimitiveObject;

class EditorManager {
public:
    enum class MapEditMode {
        Normal,
        Select,
        Copy,
        Paste,
        BucketFill
    };

    // 初期化 (ImGuiのセットアップ)
    void Initialize(HWND hwnd, ID3D12Device *device, ID3D12CommandQueue *commandQueue);

    // 毎フレームのUI構築前処理
    void BeginFrame();

    // 実際のUI構築 (ライトやカメラの調整)
    // 💡 値を書き換えるため、ポインタや参照を受け取ります
    void UpdateUI(ModelCommon *modelCommon, GameCamera *gameCamera, DebugCamera *debugCamera, Camera **activeCamera, bool &isDebugCameraActive, D3D12_GPU_DESCRIPTOR_HANDLE renderTextureSrvHandle, SceneManager *sceneManager);

    // 描画処理 (コマンドリストへImGuiの描画命令を積む)
    void Draw();

    // 終了処理 (ImGuiの解放)
    void Finalize();

    // 再生状態の取得・設定
    static bool IsPlaying() { return isPlaying_; }
    static void SetPlaying(bool isPlaying) { isPlaying_ = isPlaying; }

    bool IsGameViewHovered() const { return isGameViewHovered_; }
    bool IsReplayEditorHovered() const { return isReplayEditorHovered_; }
    bool IsAnimationEditorHovered() const { return isAnimationEditorHovered_; }
    bool IsMapEditorVisible() const { return isMapEditorVisible_; }
    bool IsMapEditorHovered() const { return isMapEditorHovered_; }
    bool IsRoomDragging() const { return draggingRoomIndex_ != -1; }

    static bool IsShowObjects() { return showObjects_; }
    static bool IsShowEffects() { return showEffects_; }

    bool UseDebugCamera() const { return useDebugCamera_; }
    void SetUseDebugCamera(bool use) { useDebugCamera_ = use; }

    bool IsTakeoverCountdown() const { return takeoverCountdown_ > 0.0f; }

    // ウィンドウレイアウトプリセット構造体
    struct WindowLayoutPreset {
        std::string name;
        std::string iniData; // ImGui の INI 文字列
        bool showInspector = true;
        bool showHierarchy = true;
        bool showGameView = true;
        bool showPostEffect = true;
        bool showMapEditor = true;
        bool showMapSettings = true;
        bool showReplayEditor = true;
        bool showAnimEditor = true;
    };

    // レイアウトプリセットの保存・読込み・管理
    void ScanLayoutPresets();
    void SaveLayoutPreset(const std::string& name);
    bool ApplyLayoutPreset(const std::string& name);
    bool DeleteLayoutPreset(const std::string& name);
    bool ExportLayoutPresetToFile(const std::string& name, const std::string& filePath);
    bool ImportLayoutPresetFromFile(const std::string& filePath);
    void ApplyDefaultLayout();
    const std::vector<WindowLayoutPreset>& GetLayoutPresets() const { return layoutPresets_; }

    // シーン設定のJSON保存・読込み
    void SaveSceneConfig();
    void LoadSceneConfig();

    // ライティング設定のJSON保存・読込み
    void SaveLightingConfig(ModelCommon* modelCommon);
    void LoadLightingConfig(ModelCommon* modelCommon);

    // 現在選択中のシーンタイプを取得
    SceneType GetCurrentSceneType() const { return currentSceneType_; }

    // 今読み込んでいるマップから物理A* (詰みチェック)のスタート・ゴール座標を自動更新
    void UpdateAStarPositionsFromMap(class MapChip2D* mapChip, class SceneManager* sceneManager = nullptr);


    // リプレイノード（キー入力ブロック）選択構造体
    struct SelectedReplayBlock {
        int trackIdx = -1;      // トラックインデックス (0~6)
        int startFrame = -1;    // 開始フレーム
        int endFrame = -1;      // 終了フレーム (排他、startFrame <= f < endFrame)

        bool IsValid() const {
            return trackIdx >= 0 && trackIdx < 7 && startFrame >= 0 && endFrame > startFrame;
        }
        bool Equals(int t, int s, int e) const {
            return trackIdx == t && startFrame == s && endFrame == e;
        }
        void Clear() {
            trackIdx = -1;
            startFrame = -1;
            endFrame = -1;
        }
    };

    enum class ReplayBlockDragMode {
        None,
        Move,         // ノード全体の移動
        ResizeLeft,   // 左端リサイズ（開始フレーム変更）
        ResizeRight   // 右端リサイズ（終了フレーム変更）
    };

    // 選択状態のクリア (シーン再生成時にワイルドポインタになるのを防ぐ)
    void ClearSelection() {
        selectedObject_ = nullptr;
        selectedGameObject_ = nullptr;
        selectedParticle_ = nullptr;
        selectedPrimitive_ = nullptr;
        selectedReplayBlock_.Clear();
        sceneJustReset_ = true; // シーンリセットのフラグを立てる
        ClearHistory();
    }

    class IEditorCommand {
    public:
        virtual ~IEditorCommand() = default;
        virtual void Undo() = 0;
        virtual void Redo() = 0;
    };

    template <typename T>
    class ValueEditCommand : public IEditorCommand {
        T* target_;
        T oldValue_;
        T newValue_;
        std::function<void()> onUpdate_;
    public:
        ValueEditCommand(T* target, const T& oldVal, const T& newVal, std::function<void()> onUpdate = nullptr)
            : target_(target), oldValue_(oldVal), newValue_(newVal), onUpdate_(onUpdate) {}
        void Undo() override { *target_ = oldValue_; if (onUpdate_) onUpdate_(); }
        void Redo() override { *target_ = newValue_; if (onUpdate_) onUpdate_(); }
    };
    
    template <typename T>
    void PushCommand(T* target, const T& oldVal, const T& newVal, std::function<void()> onUpdate = nullptr) {
        undoStack_.push_back(std::make_shared<ValueEditCommand<T>>(target, oldVal, newVal, onUpdate));
        if (undoStack_.size() > 100) {
            undoStack_.erase(undoStack_.begin());
        }
        redoStack_.clear();
    }

    class ActionCommand : public IEditorCommand {
        std::function<void()> undoAction_;
        std::function<void()> redoAction_;
    public:
        ActionCommand(std::function<void()> undo, std::function<void()> redo)
            : undoAction_(undo), redoAction_(redo) {}
        void Undo() override { if(undoAction_) undoAction_(); }
        void Redo() override { if(redoAction_) redoAction_(); }
    };

    void PushActionCommand(std::function<void()> undo, std::function<void()> redo) {
        undoStack_.push_back(std::make_shared<ActionCommand>(undo, redo));
        if (undoStack_.size() > 100) {
            undoStack_.erase(undoStack_.begin());
        }
        redoStack_.clear();
    }

    void PushCommand(std::shared_ptr<IEditorCommand> cmd) {
        undoStack_.push_back(cmd);
        if (undoStack_.size() > 100) {
            undoStack_.erase(undoStack_.begin());
        }
        redoStack_.clear();
    }

    void Undo();
    void Redo();
    void ClearHistory() {
        undoStack_.clear();
        redoStack_.clear();
    }

    // マップ用の履歴保存ヘルパー
    void BeginMapHistoryCapture(class MapChip2D* mapChip);
    void EndMapHistoryCapture(class MapChip2D* mapChip);
    
    // バウンダリ用の履歴保存ヘルパー
    void BeginRoomHistoryCapture(class MapChip2D* mapChip);
    void EndRoomHistoryCapture(class MapChip2D* mapChip);

    static ImVec2 GetGameViewPos() { return gameViewPos_; }
    static ImVec2 GetGameViewSize() { return gameViewSize_; }

private:
    static ImVec2 gameViewPos_;
    static ImVec2 gameViewSize_;

    bool sceneJustReset_ = false;
    bool loadMapDataStrNextFrame_ = false;
    std::string mapDataStrToLoad_ = "";
    std::vector<StageRoom> savedRoomsForPlay_;
    char stageFilename_[128] = "map_data.txt";

    static bool isPlaying_; // ゲーム再生中かどうか
    bool useDebugCamera_ = true; // デバッグカメラを使用するかどうか
    float takeoverCountdown_ = 0.0f; // 操作引き継ぎ時のカウントダウン

    // 選択中のオブジェクト
    Object3D *selectedObject_ = nullptr;
    std::shared_ptr<GameObject> selectedGameObject_ = nullptr;
    ParticleManager *selectedParticle_ = nullptr;
    PrimitiveObject *selectedPrimitive_ = nullptr;
    bool selectedReplaySeekbar_ = false; // シークバー/リプレイ全般が選択されているかのフラグ


    bool isGameViewHovered_ = false; // ゲームビューがホバーされているか
    bool isReplayEditorHovered_ = false; // リプレイエディタがホバーされているか
    bool isAnimationEditorHovered_ = false; // アニメーションエディタがホバーされているか
    bool isAnimationScenePushed_ = false; // アニメーション専用シーンがスタックにプッシュされているか
    bool isMapEditorVisible_ = false; // マップエディタがアクティブタブとして表示されているか
    bool wasMapEditorVisible_ = false; // 前フレームの表示状態
    bool isMapEditorHovered_ = false; // マップエディタがホバーされているか
    
    // マップエディタ用のツール状態
    int mapEditorSelectedTool_ = 100; // 0 = None, 100 = Custom Block 1
    int mapEditorInputWidth_ = -1;
    int mapEditorInputHeight_ = -1;
    
    // Normalモード用補間
    int prevGridX_ = -1;
    int prevGridY_ = -1;
    std::vector<std::pair<int, int>> pendingBlocks_;

    // 拡張マップエディタ状態
    MapEditMode mapEditMode_ = MapEditMode::Normal;
    int selectStartX_ = -1;
    int selectStartY_ = -1;
    int selectEndX_ = -1;
    int selectEndY_ = -1;
    std::vector<std::vector<int>> clipboardMapData_;

    // 範囲移動用
    bool isDraggingSelection_ = false;
    int dragStartGridX_ = -1;
    int dragStartGridY_ = -1;
    int originalSelectStartX_ = -1;
    int originalSelectStartY_ = -1;
    int originalSelectEndX_ = -1;
    int originalSelectEndY_ = -1;
    std::vector<std::vector<int>> dragSelectionData_;

    // 境界線編集用
    bool isRoomEditMode_ = false;
    int draggingRoomIndex_ = -1;
    int roomDragHandle_ = 0; // 0: None, 1: Move, 2: TopLeft, 3: TopRight, 4: BottomLeft, 5: BottomRight, 6: Left, 7: Right, 8: Top, 9: Bottom
    float roomDragOffsetX_ = 0.0f;
    float roomDragOffsetY_ = 0.0f;

    std::vector<std::shared_ptr<IEditorCommand>> undoStack_;
    std::vector<std::shared_ptr<IEditorCommand>> redoStack_;
    
    public:
    struct MapState {
        int width, height;
        std::vector<std::vector<int>> data;
    };
    
    struct RoomState {
        std::vector<struct StageRoom> rooms;
    };
    private:
    MapState oldMapState_;
    RoomState oldRoomState_;

    std::set<std::string> customToolFilters_;
    std::vector<std::string> availableModels_; // "Object/..." のような相対パスを保持
    void ScanAvailableModels();
    
    std::vector<std::string> availableTextures_;
    void ScanAvailableTextures();

    // エディターで選択中のシーンタイプ
    SceneType currentSceneType_ = SceneType::kTitle;

    // グローバルライティング設定状態
    int activeLightType_ = 2;
    bool enableFog_ = false;
    float dIntensity_ = 1.0f;
    float pIntensity_ = 1.0f;
    float sIntensity_ = 4.0f;
    float spotAngleDeg_ = 30.0f;
    float spotFalloffDeg_ = 20.0f;
    bool enableFlatShading_ = false;

    enum class EditorMode {
        Normal,
        Replay,
        Animation
    };
    EditorMode currentMode_ = EditorMode::Normal;

    // 各ウィンドウの表示状態フラグ
    bool showInspector_ = true;
    bool showHierarchy_ = true;
    bool showGameView_ = true;
    bool showPostEffect_ = true;
    bool showReplayEditor_ = true;
    bool showMapEditor_ = true;
    bool showMapSettings_ = true;
    bool showAnimEditor_ = true;

    // アニメーションエディター（Blender風ドープシート）用変数
    int animEditorTargetAnim_ = 0; // 0: WallClimb, 1: AirDash, 2: Custom...
    float animEditorTime_ = 0.0f;
    bool animEditorPlaying_ = false;
    bool animEditorLoop_ = true;
    float animEditorFps_ = 60.0f;
    std::string animEditorSelectedJointName_ = "Hips_01";
    int animEditorSelectedProperty_ = 0; // 0: Rotation, 1: Translation, 2: Scale
    int animEditorSelectedKeyIndex_ = -1;
    bool isDraggingAnimKeyframe_ = false;
    float dragAnimKeyOriginalTime_ = 0.0f;
    bool isSummaryKeyDrag_ = false;
    float dragSummaryOriginalTime_ = 0.0f;
    bool isAnimRulerScrubbing_ = false;
    float animTimelineZoom_ = 200.0f; // 1秒あたりのピクセル幅
    float animTimelineScrollX_ = 0.0f;
    
    // ボーン階層ツリー構造用
    struct AnimJointTreeNode {
        std::string name;
        int32_t jointIndex = -1;
        int32_t parentIndex = -1;
        std::vector<int32_t> children;
        int depth = 0;
    };

    Animation editingAnimation_;
    bool animEditorInitialized_ = false;
    std::vector<std::string> currentJointList_;
    std::vector<AnimJointTreeNode> animJointTreeNodes_;
    std::vector<int32_t> animJointRootIndices_;
    std::unordered_map<std::string, bool> animJointExpanded_;
    std::vector<std::string> availableAnimationFiles_;
    std::string currentAnimFilePath_ = "resources/json/shared/Player/wall_climb_animation.json";
    char newAnimSaveNameBuf_[128] = "";
    bool openSaveAnimModal_ = false;
    bool openDeleteAnimModal_ = false;

    // カメラ軸スナップの線形補間用変数
    bool isCameraSnapLerping_ = false;
    float cameraSnapLerpTimer_ = 0.0f;
    float cameraSnapLerpDuration_ = 0.25f;
    Vector3 cameraSnapStartRot_ = {};
    Vector3 cameraSnapEndRot_ = {};
    Vector3 cameraSnapStartPos_ = {};
    Vector3 cameraSnapEndPos_ = {};
    
    void DrawAnimationEditorMainView(class SceneManager* sceneManager, Camera** activeCamera, D3D12_GPU_DESCRIPTOR_HANDLE renderTextureSrvHandle);
    void DrawAnimationViewportGrid(const Matrix4x4& viewProjectionMatrix, ImVec2 vpPos, ImVec2 vpSize);
    void DrawCameraOrientationGizmo(Camera* activeCamera, ImVec2 vpPos, ImVec2 vpSize);
    void DrawSkeletonJointsOverlay(class SceneManager* sceneManager, Camera* activeCamera, ImVec2 vpPos, ImVec2 vpSize);
    void DrawBoneTransformGizmo(class SceneManager* sceneManager, Camera* activeCamera, ImVec2 vpPos, ImVec2 vpSize);
    void DrawAnimationDopeSheetUI(class SceneManager* sceneManager);
    void DrawAnimationInspectorUI(class SceneManager* sceneManager);
    void RefreshAnimationJointList(class SceneManager* sceneManager);
    void UpdateAnimationPosePreview(class SceneManager* sceneManager);
    void ScanAnimationFiles();
    class AnimatorComponent* GetTargetAnimator(class SceneManager* sceneManager);

    // 未挿入時の一時プレビューポーズ用バッファ
    struct TempBoneOverride {
        std::optional<Vector3> translate;
        std::optional<Quaternion> rotate;
        std::optional<Vector3> scale;
    };
    std::unordered_map<std::string, TempBoneOverride> animTempOverrides_;

    // ボーン用 3D ギズモパラメータ
    int animGizmoMode_ = 1; // 0: Translation (T), 1: Rotation (R), 2: Scale (S)
    int animGizmoSpace_ = 0; // 0: Local, 1: World
    int animGizmoActiveAxis_ = -1; // -1: None, 0: X, 1: Y, 2: Z, 3: Center/XYZ
    bool isDraggingAnimGizmo_ = false;
    bool isAnimLocked_ = false; // Lキーで切替可能な編集ロック状態フラグ
    bool isAnimHudMinimized_ = false; // ビューポートHUDの縮小化・最小化フラグ (Hキーで切替)
    ImVec2 animGizmoDragStartMouse_ = {};
    Vector3 animGizmoStartTranslate_ = {};
    Quaternion animGizmoStartRotate_ = {};
    Vector3 animGizmoStartScale_ = { 1.0f, 1.0f, 1.0f };

    // アニメーションエディター Undo / Redo
    struct AnimEditorSnapshot {
        Animation animation;
        float time = 0.0f;
        std::string selectedJointName;
        int selectedKeyIndex = -1;
        std::string description;
    };

    std::vector<AnimEditorSnapshot> animUndoStack_;
    std::vector<AnimEditorSnapshot> animRedoStack_;
    AnimEditorSnapshot animDragPreSnapshot_;
    bool hasAnimDragPreSnapshot_ = false;

    void PushAnimUndoState(const std::string& desc = "");
    void PerformAnimUndo(class SceneManager* sceneManager);
    void PerformAnimRedo(class SceneManager* sceneManager);
    void ClearAnimUndoRedo();

    // アニメーション対称編集（ミラー編集）
    bool animSymmetryMode_ = true; // デフォルトで対称編集ON
    bool animSymmetryAxisX_ = true;  // X軸 (左右対称)
    bool animSymmetryAxisY_ = false; // Y軸 (上下対称)
    bool animSymmetryAxisZ_ = false; // Z軸 (前後対称)
    std::map<std::string, std::string> customSymmetryMap_; // ユーザー手動指定マッピング
    std::string FindOppositeJointName(const std::string& jointName, bool axisX = true, bool axisY = false, bool axisZ = false, const struct Skeleton* skeleton = nullptr);

    // タイムライン（リプレイエディター）用パラメータ
    float timelineZoom_ = 4.0f;     // 1フレームあたりのピクセル幅
    float timelineScrollX_ = 0.0f;  // タイムライン横スクロール位置

    // リプレイノード選択・ドラッグ状態
    SelectedReplayBlock selectedReplayBlock_;
    ReplayBlockDragMode replayBlockDragMode_ = ReplayBlockDragMode::None;
    ReplayBlockDragMode pendingBlockDragMode_ = ReplayBlockDragMode::None; // 閾値判定用のドラッグ予備状態
    ImVec2 dragStartMousePos_ = ImVec2(0.0f, 0.0f);                      // クリック開始位置
    SelectedReplayBlock draggingBlockOriginal_; // ドラッグ開始時の元ブロック
    int dragStartMouseFrame_ = 0;               // ドラッグ開始時のマウス位置フレーム
    bool isRulerScrubbing_ = false;             // ルーラーでのシークドラッグ中フラグ

    // リプレイ Undo 用一時保存
    ReplayData replayDragOldReplayData_;
    SelectedReplayBlock replayDragOldSelectedBlock_;

    // リプレイ保存ダイアログ用
    int saveTargetHistoryIdx_ = -1;
    char saveFileNameBuf_[128] = "replay_1";

    // 物理A* (詰みチェック)用座標
    float aStarStartPos_[2] = { 0.0f, 0.0f };
    float aStarGoalPos_[2] = { 30.0f, 0.0f };
    bool isAStarPosInitialized_ = false;

    // レイアウトプリセット用メンバ変数
    std::vector<WindowLayoutPreset> layoutPresets_;
    bool showSavePresetWindow_ = false;
    char newPresetNameBuf_[128] = "";
    std::string presetStatusMessage_ = "";
    float presetStatusMessageTimer_ = 0.0f;

    static bool showObjects_;
    static bool showEffects_;
};
#endif