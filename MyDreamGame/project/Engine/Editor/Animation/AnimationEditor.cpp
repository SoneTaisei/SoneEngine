#ifdef USE_IMGUI
#include "AnimationEditor.h"

AnimationEditor::AnimationEditor() {
    context_ = std::make_unique<AnimationEditorContext>();
    viewport_ = std::make_unique<AnimationEditorViewport>();
    dopeSheet_ = std::make_unique<AnimationDopeSheet>();
    inspector_ = std::make_unique<AnimationInspector>();
    Initialize();
}

void AnimationEditor::Initialize() {
    if (context_) context_->Initialize();
    if (viewport_) viewport_->Initialize();
    if (dopeSheet_) dopeSheet_->Initialize();
    if (inspector_) inspector_->Initialize();
}

void AnimationEditor::UpdateAnimationPosePreview(SceneManager* sceneManager) {
    if (context_) context_->UpdateAnimationPosePreview(sceneManager);
}

void AnimationEditor::DrawMainView(SceneManager* sceneManager, Camera** activeCamera, D3D12_GPU_DESCRIPTOR_HANDLE renderTextureSrvHandle) {
    if (viewport_ && context_) {
        viewport_->DrawMainView(sceneManager, activeCamera, renderTextureSrvHandle, context_.get());
    }
}

void AnimationEditor::DrawDopeSheetUI(SceneManager* sceneManager) {
    if (dopeSheet_ && context_) {
        dopeSheet_->DrawDopeSheetUI(sceneManager, context_.get());
    }
}

void AnimationEditor::DrawInspectorUI(SceneManager* sceneManager) {
    if (inspector_ && context_) {
        inspector_->DrawInspectorUI(sceneManager, context_.get());
    }
}

void AnimationEditor::SetSelectedTargets(Object3D* obj, std::shared_ptr<GameObject> gameObj, PrimitiveObject* prim) {
    if (context_) {
        context_->SetSelectedTargets(obj, gameObj, prim);
    }
}

void AnimationEditor::RefreshAnimationJointList(SceneManager* sceneManager) {
    if (context_) {
        context_->RefreshAnimationJointList(sceneManager);
    }
}

bool AnimationEditor::IsHovered() const {
    return context_ ? context_->IsHovered() : false;
}

void AnimationEditor::SetHovered(bool hovered) {
    if (context_) context_->SetHovered(hovered);
}

bool& AnimationEditor::GetShowAnimEditor() {
    static bool fallback = false;
    return context_ ? context_->GetShowAnimEditor() : fallback;
}

void AnimationEditor::SetShowAnimEditor(bool show) {
    if (context_) context_->SetShowAnimEditor(show);
}

bool AnimationEditor::IsAnimScenePushed() const {
    return context_ ? context_->IsAnimScenePushed() : false;
}

void AnimationEditor::SetAnimScenePushed(bool pushed) {
    if (context_) context_->SetAnimScenePushed(pushed);
}

const std::string& AnimationEditor::GetSelectedJointName() const {
    static const std::string empty = "";
    return context_ ? context_->GetSelectedJointName() : empty;
}

void AnimationEditor::SetSelectedJointName(const std::string& name) {
    if (context_) context_->SetSelectedJointName(name);
}

const std::vector<std::string>& AnimationEditor::GetCurrentJointList() const {
    static const std::vector<std::string> empty;
    return context_ ? context_->GetCurrentJointList() : empty;
}

void AnimationEditor::PushAnimUndoState(const std::string& desc) {
    if (context_) context_->PushAnimUndoState(desc);
}

void AnimationEditor::PerformAnimUndo(SceneManager* sceneManager) {
    if (context_) context_->PerformAnimUndo(sceneManager);
}

void AnimationEditor::PerformAnimRedo(SceneManager* sceneManager) {
    if (context_) context_->PerformAnimRedo(sceneManager);
}

void AnimationEditor::ClearAnimUndoRedo() {
    if (context_) context_->ClearAnimUndoRedo();
}

AnimatorComponent* AnimationEditor::GetTargetAnimator(SceneManager* sceneManager) {
    return context_ ? context_->GetTargetAnimator(sceneManager) : nullptr;
}

#endif
