#pragma once
#ifdef USE_IMGUI
#include <string>
#include <vector>
#include <set>
#include <memory>
#include <functional>
#include <imgui.h>
#include "Game2D/MapChip2D.h"

class SceneManager;

class MapEditorContext {
public:
    enum class MapEditMode {
        Normal,
        Select,
        Copy,
        Paste,
        BucketFill
    };

    struct MapState {
        int width = 0;
        int height = 0;
        std::vector<std::vector<int>> data;
    };

    struct RoomState {
        std::vector<StageRoom> rooms;
    };

    class IMapCommand {
    public:
        virtual ~IMapCommand() = default;
        virtual void Undo() = 0;
        virtual void Redo() = 0;
    };

    MapEditorContext();
    ~MapEditorContext() = default;

    void Initialize();
    void ScanAvailableModels();
    void ScanAvailableTextures();

    // 編集モード
    MapEditMode GetEditMode() const { return mapEditMode_; }
    void SetEditMode(MapEditMode mode) { mapEditMode_ = mode; }

    // ツール選択
    int GetSelectedTool() const { return selectedTool_; }
    void SetSelectedTool(int toolId) { selectedTool_ = toolId; }

    // 範囲選択
    int GetSelectStartX() const { return selectStartX_; }
    int GetSelectStartY() const { return selectStartY_; }
    int GetSelectEndX() const { return selectEndX_; }
    int GetSelectEndY() const { return selectEndY_; }
    void SetSelectRect(int startX, int startY, int endX, int endY) {
        selectStartX_ = startX;
        selectStartY_ = startY;
        selectEndX_ = endX;
        selectEndY_ = endY;
    }
    void ClearSelection() {
        selectStartX_ = -1;
        selectStartY_ = -1;
        selectEndX_ = -1;
        selectEndY_ = -1;
    }
    bool HasSelection() const {
        return selectStartX_ != -1 && selectStartY_ != -1 && selectEndX_ != -1 && selectEndY_ != -1;
    }

    // クリップボード
    const std::vector<std::vector<int>>& GetClipboardData() const { return clipboardMapData_; }
    std::vector<std::vector<int>>& GetClipboardData() { return clipboardMapData_; }
    void SetClipboardData(const std::vector<std::vector<int>>& data) { clipboardMapData_ = data; }

    // 範囲ドラッグ状態
    bool IsDraggingSelection() const { return isDraggingSelection_; }
    void SetDraggingSelection(bool dragging) { isDraggingSelection_ = dragging; }
    int GetDragStartGridX() const { return dragStartGridX_; }
    int GetDragStartGridY() const { return dragStartGridY_; }
    void SetDragStartGrid(int x, int y) { dragStartGridX_ = x; dragStartGridY_ = y; }

    // ペン描画補間
    int GetPrevGridX() const { return prevGridX_; }
    int GetPrevGridY() const { return prevGridY_; }
    void SetPrevGrid(int x, int y) { prevGridX_ = x; prevGridY_ = y; }
    std::vector<std::pair<int, int>>& GetPendingBlocks() { return pendingBlocks_; }

    // ルーム編集
    bool IsRoomEditMode() const { return isRoomEditMode_; }
    void SetRoomEditMode(bool mode) { isRoomEditMode_ = mode; }
    int GetDraggingRoomIndex() const { return draggingRoomIndex_; }
    void SetDraggingRoomIndex(int idx) { draggingRoomIndex_ = idx; }
    int GetRoomDragHandle() const { return roomDragHandle_; }
    void SetRoomDragHandle(int handle) { roomDragHandle_ = handle; }
    float GetRoomDragOffsetX() const { return roomDragOffsetX_; }
    float GetRoomDragOffsetY() const { return roomDragOffsetY_; }
    void SetRoomDragOffset(float x, float y) { roomDragOffsetX_ = x; roomDragOffsetY_ = y; }

    // ファイル名・サイズ入力
    const char* GetStageFilename() const { return stageFilename_; }
    char* GetStageFilenameBuffer() { return stageFilename_; }
    void SetStageFilename(const std::string& filename);
    std::string GetFullFilePath(const char* filename) const;

    int GetInputWidth() const { return inputWidth_; }
    int GetInputHeight() const { return inputHeight_; }
    void SetInputSize(int w, int h) { inputWidth_ = w; inputHeight_ = h; }
    int* GetInputWidthPtr() { return &inputWidth_; }
    int* GetInputHeightPtr() { return &inputHeight_; }

    // フィルター
    std::set<std::string>& GetCustomToolFilters() { return customToolFilters_; }
    const std::set<std::string>& GetCustomToolFilters() const { return customToolFilters_; }

    // リソースリスト
    const std::vector<std::string>& GetAvailableModels() const { return availableModels_; }
    const std::vector<std::string>& GetAvailableTextures() const { return availableTextures_; }

    // Undo / Redo
    void PushCommand(std::shared_ptr<IMapCommand> cmd);
    void Undo();
    void Redo();
    void ClearHistory();

    void BeginMapHistoryCapture(MapChip2D* mapChip);
    void EndMapHistoryCapture(MapChip2D* mapChip);
    void BeginRoomHistoryCapture(MapChip2D* mapChip);
    void EndRoomHistoryCapture(MapChip2D* mapChip);

    // 物理A* 座標自動更新
    void UpdateAStarPositionsFromMap(MapChip2D* mapChip, SceneManager* sceneManager = nullptr);
    float GetAStarStartX() const { return aStarStartPos_[0]; }
    float GetAStarStartY() const { return aStarStartPos_[1]; }
    float GetAStarGoalX() const { return aStarGoalPos_[0]; }
    float GetAStarGoalY() const { return aStarGoalPos_[1]; }
    void SetAStarStartPos(float x, float y) { aStarStartPos_[0] = x; aStarStartPos_[1] = y; }
    void SetAStarGoalPos(float x, float y) { aStarGoalPos_[0] = x; aStarGoalPos_[1] = y; }

    // グリッド線表示設定
    bool IsShowGrid() const { return showGrid_; }
    void SetShowGrid(bool show) { showGrid_ = show; }
    bool* GetShowGridPtr() { return &showGrid_; }

private:
    bool showGrid_ = true;
    MapEditMode mapEditMode_ = MapEditMode::Normal;
    int selectedTool_ = 100; // 0 = None, 100 = Custom Block 1

    // 範囲選択
    int selectStartX_ = -1;
    int selectStartY_ = -1;
    int selectEndX_ = -1;
    int selectEndY_ = -1;
    std::vector<std::vector<int>> clipboardMapData_;

    // 範囲ドラッグ
    bool isDraggingSelection_ = false;
    int dragStartGridX_ = -1;
    int dragStartGridY_ = -1;

    // Normalモード補間用
    int prevGridX_ = -1;
    int prevGridY_ = -1;
    std::vector<std::pair<int, int>> pendingBlocks_;

    // ルーム編集
    bool isRoomEditMode_ = false;
    int draggingRoomIndex_ = -1;
    int roomDragHandle_ = 0; // 0: None, 1: Move, 2: TopLeft, 3: TopRight, 4: BottomLeft, 5: BottomRight, 6: Left, 7: Right, 8: Top, 9: Bottom
    float roomDragOffsetX_ = 0.0f;
    float roomDragOffsetY_ = 0.0f;

    // ファイルとサイズ
    char stageFilename_[128] = "map_data.txt";
    int inputWidth_ = -1;
    int inputHeight_ = -1;

    // フィルター
    std::set<std::string> customToolFilters_;

    // リソース一覧
    std::vector<std::string> availableModels_;
    std::vector<std::string> availableTextures_;

    // 履歴管理
    MapState oldMapState_;
    RoomState oldRoomState_;
    std::vector<std::shared_ptr<IMapCommand>> undoStack_;
    std::vector<std::shared_ptr<IMapCommand>> redoStack_;

    // A*座標
    float aStarStartPos_[2] = { 0.0f, 0.0f };
    float aStarGoalPos_[2] = { 30.0f, 0.0f };
};
#endif
