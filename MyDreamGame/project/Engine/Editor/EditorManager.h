#pragma once
#ifdef USE_IMGUI
#include <Windows.h>
#include <d3d12.h>
#include <cstdint>
#include <set>
#include <unordered_map>
#include <memory>
#include <functional>
#include <vector>
#include <string>

// UIから操作したいクラスのヘッダーをインクルード
#include "Graphics/DebugCamera.h"
#include "Graphics/GameCamera.h"
#include "Resource/Model/ModelCommon.h"
#include "Scene/SceneFactory.h"
#include "Core/Utility/Structs.h"
#include "Replay/ReplayManager.h"
#include "Animation/AnimationEditor.h"
#include "MapEditor/MapEditor.h"
#include "LightEditor/LightEditor.h"
#include "Model3DEditor/Model3DEditor.h"
#include "PostEffectEditor/PostEffectEditor.h"

class SceneManager;
class ParticleManager;
class Object3D;
class PrimitiveObject;
class IScene;

class EditorManager {
public:
    void LoadPlacedModelsForScene(IScene* scene);
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

    // 3Dオブジェクト描画処理 (RenderTextureへの3Dモデル配置オブジェクト描画)
    void Draw3D();

    // 終了処理 (ImGuiの解放)
    void Finalize();

    // シングルトンインスタンス取得
    static EditorManager* GetInstance() { return s_Instance; }

    // 再生状態の取得・設定
    static bool IsPlaying() { return isPlaying_; }
    static void SetPlaying(bool isPlaying) { isPlaying_ = isPlaying; }
    static bool IsPaused() { return isPaused_; }
    static void SetPaused(bool isPaused) { isPaused_ = isPaused; }

    // マップエディターの表示切り替え・フォーカス制御
    void ToggleMapEditor();
    void FocusMapEditor();
    void FocusGameView();

    // プレイ中のマップ変更を一時保存データに同期（Stop時の復元用）
    void SyncPlayMapData(class MapChip2D* mapChip);

    bool IsGameViewHovered() const { return isGameViewHovered_; }
    bool IsReplayEditorHovered() const { return isReplayEditorHovered_; }
    bool IsAnimationEditorHovered() const { return animationEditor_ ? animationEditor_->IsHovered() : false; }
    bool IsLightEditorHovered() const { return lightEditor_ ? lightEditor_->IsHovered() : false; }
    bool IsModel3DEditorHovered() const { return model3DEditor_ ? model3DEditor_->IsHovered() : false; }
    bool IsMapEditorVisible() const { return mapEditor_ ? mapEditor_->IsVisible() : false; }
    bool IsMapEditorHovered() const { return mapEditor_ ? mapEditor_->IsHovered() : false; }
    bool IsRoomDragging() const { return mapEditor_ ? mapEditor_->IsRoomDragging() : false; }
    const std::string& GetActiveMainTab() const { return activeMainTab_; }

    static bool IsShowObjects() { return showObjects_; }
    static bool IsShowEffects() { return showEffects_; }

    bool UseDebugCamera() const { return useDebugCamera_; }
    void SetUseDebugCamera(bool use) { useDebugCamera_ = use; }
    class DebugCamera* GetDebugCamera() const { return currentDebugCamera_; }

    bool IsTakeoverCountdown() const { return takeoverCountdown_ > 0.0f; }

    AnimationEditor* GetAnimationEditor() const { return animationEditor_.get(); }
    MapEditor* GetMapEditor() const { return mapEditor_.get(); }
    LightEditor* GetLightEditor() const { return lightEditor_.get(); }
    Model3DEditor* GetModel3DEditor() const { return model3DEditor_.get(); }
    PostEffectEditor* GetPostEffectEditor() const { return postEffectEditor_.get(); }

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
        bool showLightEditor = true;
        bool showSpotLightPanel = true;
        bool showModelPlacement = true;
        bool showModelPalette = true;
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

    // 現在選択中のステージマップファイル名を取得
    const char* GetStageFilename() const { return mapEditor_ ? mapEditor_->GetStageFilename() : ""; }

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
        if (animationEditor_) {
            animationEditor_->SetSelectedTargets(nullptr, nullptr, nullptr);
        }
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
    void BeginMapHistoryCapture(class MapChip2D* mapChip) { if (mapEditor_) mapEditor_->BeginMapHistoryCapture(mapChip); }
    void EndMapHistoryCapture(class MapChip2D* mapChip) { if (mapEditor_) mapEditor_->EndMapHistoryCapture(mapChip); }
    
    // バウンダリ用の履歴保存ヘルパー
    void BeginRoomHistoryCapture(class MapChip2D* mapChip) { if (mapEditor_) mapEditor_->BeginRoomHistoryCapture(mapChip); }
    void EndRoomHistoryCapture(class MapChip2D* mapChip) { if (mapEditor_) mapEditor_->EndRoomHistoryCapture(mapChip); }

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
    static bool isPaused_;  // ゲーム再生中の一時停止かどうか
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

    std::vector<std::shared_ptr<IEditorCommand>> undoStack_;
    std::vector<std::shared_ptr<IEditorCommand>> redoStack_;

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
        Animation,
        Light,
        ModelPlacement
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
    bool showLightEditor_ = true;
    bool showSpotLightPanel_ = true;
    bool showModelPlacementEditor_ = true;
    bool showModelPalette_ = true;

    // 前回選択されていたメインタブ（次回起動時に復元）
    std::string activeMainTab_ = "ゲームビュー";
    int focusActiveTabCountdown_ = 0;
    int focusSpotLightTabCountdown_ = 0;

    // サブエディター専用インスタンス
    std::unique_ptr<AnimationEditor> animationEditor_;
    std::unique_ptr<MapEditor> mapEditor_;
    std::unique_ptr<LightEditor> lightEditor_;
    std::unique_ptr<Model3DEditor> model3DEditor_;
    std::unique_ptr<PostEffectEditor> postEffectEditor_;

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
    DebugCamera* currentDebugCamera_ = nullptr;

    static bool showObjects_;
    static bool showEffects_;
    static EditorManager* s_Instance;
};
#endif