#ifdef USE_IMGUI
#include "GPUParticleEditor.h"

GPUParticleEditor::GPUParticleEditor() {
    context_ = std::make_unique<GPUParticleEditorContext>();
    viewport_ = std::make_unique<GPUParticleEditorViewport>();
    inspector_ = std::make_unique<GPUParticleInspector>();
    timeline_ = std::make_unique<GPUParticleTimeline>();
}

void GPUParticleEditor::Initialize(ID3D12Device* device) {
    if (context_) context_->Initialize(device);
    if (viewport_) viewport_->Initialize();
    if (inspector_) inspector_->Initialize();
    if (timeline_) timeline_->Initialize();
}

void GPUParticleEditor::Update(float deltaTime) {
    if (context_ && context_->GetSystem()) {
        float effectiveDt = deltaTime * context_->GetPlaybackSpeed();
        context_->GetSystem()->Update(effectiveDt);
    }
}

void GPUParticleEditor::Draw(ID3D12GraphicsCommandList* commandList, const Matrix4x4& viewProjection, const Matrix4x4& cameraMatrix, ParticleCommon* particleCommon, ModelManager* modelManager) {
    if (context_ && context_->GetSystem()) {
        context_->GetSystem()->Draw(commandList, viewProjection, cameraMatrix, particleCommon, modelManager);
    }
}

void GPUParticleEditor::DrawMainView(SceneManager* sceneManager, Camera** activeCamera, D3D12_GPU_DESCRIPTOR_HANDLE renderTextureSrvHandle) {
    if (viewport_ && context_) {
        viewport_->DrawMainView(sceneManager, activeCamera, renderTextureSrvHandle, context_.get());
    }
}

void GPUParticleEditor::DrawInspectorUI(SceneManager* sceneManager) {
    if (inspector_ && context_) {
        inspector_->DrawInspectorUI(sceneManager, context_.get());
    }
}

void GPUParticleEditor::DrawTimelineUI(SceneManager* sceneManager) {
    if (timeline_ && context_) {
        timeline_->DrawTimelineUI(sceneManager, context_.get());
    }
}

bool GPUParticleEditor::IsHovered() const {
    return context_ ? context_->IsHovered() : false;
}

void GPUParticleEditor::SetHovered(bool hovered) {
    if (context_) context_->SetHovered(hovered);
}

bool& GPUParticleEditor::GetShowEditor() {
    static bool fallback = false;
    return context_ ? context_->GetShowEditor() : fallback;
}

void GPUParticleEditor::SetShowEditor(bool show) {
    if (context_) context_->SetShowEditor(show);
}

bool GPUParticleEditor::IsParticleScenePushed() const {
    return context_ ? context_->IsParticleScenePushed() : false;
}

void GPUParticleEditor::SetParticleScenePushed(bool pushed) {
    if (context_) context_->SetParticleScenePushed(pushed);
}
#endif
