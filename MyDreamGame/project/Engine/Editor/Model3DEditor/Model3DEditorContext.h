#pragma once
#ifdef USE_IMGUI
#include <vector>
#include <memory>
#include <string>
#include <functional>
#include <d3d12.h>
#include "PlacedObject3D.h"
#include "GameObject/PrimitiveObject.h"
#include "Core/Utility/Vector3.h"

class Model3DEditorContext {
public:
    enum class GizmoMode {
        Translation = 0,
        Rotation = 1,
        Scale = 2
    };

    enum class GizmoSpace {
        World = 0,
        Local = 1
    };

    Model3DEditorContext();
    ~Model3DEditorContext();

    void Initialize(ID3D12Device* device);
    void Update();
    void Draw();

    // Object Management
    PlacedObject3D* AddObject(const std::string& name, const std::string& modelDir, const std::string& modelFileName, const Vector3& position);
    void RemoveObject(PlacedObject3D* target);
    PlacedObject3D* DuplicateObject(PlacedObject3D* target);
    void ClearObjects();

    const std::vector<std::unique_ptr<PlacedObject3D>>& GetObjects() const { return objects_; }
    std::vector<std::unique_ptr<PlacedObject3D>>& GetObjects() { return objects_; }

    PlacedObject3D* GetSelectedObject() const { return selectedObject_; }
    void SetSelectedObject(PlacedObject3D* obj) { selectedObject_ = obj; }
    void ClearSelection() { selectedObject_ = nullptr; }

    // Gizmo Settings
    GizmoMode GetGizmoMode() const { return gizmoMode_; }
    void SetGizmoMode(GizmoMode mode) { gizmoMode_ = mode; }

    GizmoSpace GetGizmoSpace() const { return gizmoSpace_; }
    void SetGizmoSpace(GizmoSpace space) { gizmoSpace_ = space; }

    bool IsSnapEnabled() const { return snapEnabled_; }
    void SetSnapEnabled(bool enabled) { snapEnabled_ = enabled; }
    float GetTranslateSnap() const { return translateSnap_; }
    float GetRotateSnapDeg() const { return rotateSnapDeg_; }
    float GetScaleSnap() const { return scaleSnap_; }

    // Save & Load
    bool SaveToFile(const std::string& filePath = "");
    bool LoadFromFile(const std::string& filePath = "");
    const std::string& GetCurrentFilePath() const { return currentFilePath_; }
    void SetCurrentFilePath(const std::string& path) { currentFilePath_ = path; }

    // Undo / Redo
    struct PlacedObjectSnapshot {
        uint64_t id = 0;
        std::string name;
        std::string modelDirectory;
        std::string modelFileName;
        Vector3 translation{ 0.0f, 0.0f, 0.0f };
        Vector3 rotation{ 0.0f, 0.0f, 0.0f };
        Vector3 scale{ 1.0f, 1.0f, 1.0f };
        Vector4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
        bool doubleSided = false;
        std::string texturePath = "";
    };

    struct Model3DEditorSnapshot {
        std::vector<PlacedObjectSnapshot> objects;
        uint64_t selectedId = 0;
    };

    Model3DEditorSnapshot CreateSnapshot() const;
    void RestoreSnapshot(const Model3DEditorSnapshot& snapshot);
    void PushUndoState();
    void PushSnapshotToUndo(const Model3DEditorSnapshot& snapshot);
    void Undo();
    void Redo();
    bool CanUndo() const { return !undoStack_.empty(); }
    bool CanRedo() const { return !redoStack_.empty(); }
    void ClearUndoRedo() { undoStack_.clear(); redoStack_.clear(); }

    ID3D12Device* GetDevice() const { return device_; }

    // Picking helper
    PlacedObject3D* PickObject(const Vector3& rayOrigin, const Vector3& rayDir, float& outDist);

private:
    ID3D12Device* device_ = nullptr;
    std::vector<std::unique_ptr<PlacedObject3D>> objects_;
    PlacedObject3D* selectedObject_ = nullptr;

    std::vector<Model3DEditorSnapshot> undoStack_;
    std::vector<Model3DEditorSnapshot> redoStack_;
    uint64_t currentFrame_ = 0;
    uint64_t lastUndoRedoFrame_ = 0;

    GizmoMode gizmoMode_ = GizmoMode::Translation;
    GizmoSpace gizmoSpace_ = GizmoSpace::World;

    bool snapEnabled_ = false;
    float translateSnap_ = 0.5f;
    float rotateSnapDeg_ = 15.0f;
    float scaleSnap_ = 0.25f;

    std::string currentFilePath_ = "resources/json/shared/LevelData/placed_models.json";

    // 3D Depth-Tested Procedural Grid Floor Object
    std::unique_ptr<class PrimitiveObject> gridFloorObj_;
};
#endif
