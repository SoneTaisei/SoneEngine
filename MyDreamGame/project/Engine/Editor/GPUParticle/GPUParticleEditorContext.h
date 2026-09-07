#pragma once
#ifdef USE_IMGUI
#include <string>
#include <vector>
#include <memory>
#include "Effect/GPUParticle/GPUParticleSystem.h"

class SceneManager;

class GPUParticleEditorContext {
public:
    GPUParticleEditorContext() = default;
    ~GPUParticleEditorContext() = default;

    void Initialize(ID3D12Device* device);

    GPUParticleSystem* GetSystem() { return system_.get(); }
    const GPUParticleSystem* GetSystem() const { return system_.get(); }

    int GetSelectedEmitterIndex() const { return selectedEmitterIndex_; }
    void SetSelectedEmitterIndex(int index) { selectedEmitterIndex_ = index; }
    GPUParticleEmitter* GetSelectedEmitter();

    // 再生コントロール
    float GetPlaybackSpeed() const { return playbackSpeed_; }
    void SetPlaybackSpeed(float speed) { playbackSpeed_ = speed; }

    // ビューポート表示オプション
    bool& GetShowFloorGrid() { return showFloorGrid_; }
    bool& GetIsDarkBackground() { return isDarkBackground_; }
    bool& GetShowShapeGizmo() { return showShapeGizmo_; }

    // ウィンドウ状態
    bool& GetShowEditor() { return showEditor_; }
    void SetShowEditor(bool show) { showEditor_ = show; }
    bool IsHovered() const { return isHovered_; }
    void SetHovered(bool hovered) { isHovered_ = hovered; }

    bool IsParticleScenePushed() const { return isParticleScenePushed_; }
    void SetParticleScenePushed(bool pushed) { isParticleScenePushed_ = pushed; }

    // ファイルI/O
    const std::string& GetCurrentFilePath() const { return currentFilePath_; }
    void SetCurrentFilePath(const std::string& path) { currentFilePath_ = path; }
    bool SaveCurrentSystem();
    bool LoadSystem(const std::string& filePath);
    void CreateNewSystem();

    // Undo / Redo
    void PushUndoState(const std::string& desc = "");
    bool CanUndo() const { return !undoStack_.empty(); }
    bool CanRedo() const { return !redoStack_.empty(); }
    void PerformUndo();
    void PerformRedo();
    void ClearUndoRedo();

    // プロジェクト内の利用可能なアセット一覧のスキャン
    void ScanAvailableAssets();
    void ScanParticleFiles();
    const std::vector<std::string>& GetAvailableModels() const { return availableModels_; }
    const std::vector<std::string>& GetAvailableTextures() const { return availableTextures_; }
    const std::vector<std::string>& GetAvailableParticleFiles() const { return availableParticleFiles_; }

private:
    ID3D12Device* device_ = nullptr;
    std::unique_ptr<GPUParticleSystem> system_;
    int selectedEmitterIndex_ = 0;

    float playbackSpeed_ = 1.0f;
    bool showFloorGrid_ = false;
    bool isDarkBackground_ = true;
    bool showShapeGizmo_ = true;

    bool showEditor_ = true;
    bool isHovered_ = false;
    bool isParticleScenePushed_ = false;

    std::string currentFilePath_ = "resources/json/shared/Particle/default_effect.json";

    // アセットリスト
    std::vector<std::string> availableModels_;
    std::vector<std::string> availableTextures_;
    std::vector<std::string> availableParticleFiles_;

    // Undo / Redo
    std::vector<GPUParticleSystemData> undoStack_;
    std::vector<GPUParticleSystemData> redoStack_;
};
#endif
