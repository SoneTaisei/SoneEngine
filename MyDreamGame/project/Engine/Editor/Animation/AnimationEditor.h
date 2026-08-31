#pragma once
#ifdef USE_IMGUI
#include <Windows.h>
#include <d3d12.h>
#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <imgui.h>
#include "Graphics/Camera.h"
#include "AnimationEditorContext.h"
#include "AnimationEditorViewport.h"
#include "AnimationDopeSheet.h"
#include "AnimationInspector.h"

class SceneManager;
class AnimatorComponent;
class Object3D;
class GameObject;
class PrimitiveObject;

class AnimationEditor {
public:
    AnimationEditor();
    ~AnimationEditor() = default;

    void Initialize();
    void UpdateAnimationPosePreview(SceneManager* sceneManager);

    // UI描画
    void DrawMainView(SceneManager* sceneManager, Camera** activeCamera, D3D12_GPU_DESCRIPTOR_HANDLE renderTextureSrvHandle);
    void DrawDopeSheetUI(SceneManager* sceneManager);
    void DrawInspectorUI(SceneManager* sceneManager);

    // 選択中ターゲットの設定
    void SetSelectedTargets(Object3D* obj, std::shared_ptr<GameObject> gameObj, PrimitiveObject* prim);

    // 状態管理
    void RefreshAnimationJointList(SceneManager* sceneManager);
    bool IsHovered() const;
    void SetHovered(bool hovered);
    bool& GetShowAnimEditor();
    void SetShowAnimEditor(bool show);
    bool IsAnimScenePushed() const;
    void SetAnimScenePushed(bool pushed);

    const std::string& GetSelectedJointName() const;
    void SetSelectedJointName(const std::string& name);
    const std::vector<std::string>& GetCurrentJointList() const;

    void PushAnimUndoState(const std::string& desc = "");
    void PerformAnimUndo(SceneManager* sceneManager);
    void PerformAnimRedo(SceneManager* sceneManager);
    void ClearAnimUndoRedo();

    AnimatorComponent* GetTargetAnimator(SceneManager* sceneManager);

    // サブコンポーネントアクセサ
    AnimationEditorContext* GetContext() { return context_.get(); }
    AnimationEditorViewport* GetViewport() { return viewport_.get(); }
    AnimationDopeSheet* GetDopeSheet() { return dopeSheet_.get(); }
    AnimationInspector* GetInspector() { return inspector_.get(); }

private:
    std::unique_ptr<AnimationEditorContext> context_;
    std::unique_ptr<AnimationEditorViewport> viewport_;
    std::unique_ptr<AnimationDopeSheet> dopeSheet_;
    std::unique_ptr<AnimationInspector> inspector_;
};
#endif
