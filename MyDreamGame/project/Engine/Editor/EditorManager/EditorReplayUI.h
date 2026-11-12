#pragma once
#ifdef USE_IMGUI
#include <imgui.h>
#include "Editor/Replay/ReplayManager.h"
#include "Editor/Replay/ReplayIO.h"

class EditorCommandManager;
class SceneManager;
class EditorManager;

class EditorReplayUI {
public:
    struct SelectedReplayBlock {
        int trackIdx = -1;
        int startFrame = -1;
        int endFrame = -1;

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
        Move,
        ResizeLeft,
        ResizeRight
    };

    EditorReplayUI() = default;
    ~EditorReplayUI() = default;

    void DrawTimelineWindow(bool* pOpen, EditorManager* editorManager, EditorCommandManager* cmdManager);
    void DrawHierarchyWindow(bool* pOpen, EditorManager* editorManager);
    void DrawReplaySettingsInspector(EditorManager* editorManager, EditorCommandManager* cmdManager, SceneManager* sceneManager);

    bool IsHovered() const { return isReplayEditorHovered_; }
    void SetHovered(bool hovered) { isReplayEditorHovered_ = hovered; }

    const SelectedReplayBlock& GetSelectedBlock() const { return selectedReplayBlock_; }
    void ClearSelection() { selectedReplayBlock_.Clear(); }

    float* GetAStarStartPos() { return aStarStartPos_; }
    float* GetAStarGoalPos() { return aStarGoalPos_; }

private:
    bool isReplayEditorHovered_ = false;
    float timelineZoom_ = 4.0f;
    float timelineScrollX_ = 0.0f;

    SelectedReplayBlock selectedReplayBlock_;
    ReplayBlockDragMode replayBlockDragMode_ = ReplayBlockDragMode::None;
    ReplayBlockDragMode pendingBlockDragMode_ = ReplayBlockDragMode::None;
    ImVec2 dragStartMousePos_ = ImVec2(0.0f, 0.0f);
    SelectedReplayBlock draggingBlockOriginal_;
    int dragStartMouseFrame_ = 0;
    bool isRulerScrubbing_ = false;

    ReplayData replayDragOldReplayData_;
    SelectedReplayBlock replayDragOldSelectedBlock_;

    int saveTargetHistoryIdx_ = -1;
    char saveFileNameBuf_[128] = "replay_1";

    float aStarStartPos_[2] = { 0.0f, 0.0f };
    float aStarGoalPos_[2] = { 30.0f, 0.0f };
};
#endif
