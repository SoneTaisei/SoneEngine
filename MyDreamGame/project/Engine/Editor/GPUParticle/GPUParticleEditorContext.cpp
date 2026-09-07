#ifdef USE_IMGUI
#include "GPUParticleEditorContext.h"
#include <filesystem>
#include <algorithm>

void GPUParticleEditorContext::Initialize(ID3D12Device* device) {
    device_ = device;
    system_ = std::make_unique<GPUParticleSystem>();
    system_->Initialize(device_);
    system_->Pause(); // 初期は停止状態

    // デフォルト保存先ディレクトリを確保
    std::filesystem::create_directories("resources/json/shared/Particle");

    ScanAvailableAssets();
    ScanParticleFiles();
}

GPUParticleEmitter* GPUParticleEditorContext::GetSelectedEmitter() {
    if (!system_) return nullptr;
    return system_->GetEmitter(static_cast<size_t>(selectedEmitterIndex_));
}

void GPUParticleEditorContext::CreateNewSystem() {
    if (system_ && device_) {
        PushUndoState("Create New System");
        system_->Initialize(device_);
        system_->Pause(); // 新規作成時も停止状態
        selectedEmitterIndex_ = 0;
        currentFilePath_ = "resources/json/shared/Particle/new_effect.json";
        ClearUndoRedo();
    }
}

bool GPUParticleEditorContext::SaveCurrentSystem() {
    if (!system_) return false;
    // システム名が設定されていれば、その名前でファイルパスを更新
    const std::string& name = system_->GetData().systemName;
    if (!name.empty()) {
        currentFilePath_ = "resources/json/shared/Particle/" + name + ".json";
    }
    std::filesystem::path p(currentFilePath_);
    if (p.has_parent_path()) {
        std::filesystem::create_directories(p.parent_path());
    }
    bool success = system_->SaveToFile(currentFilePath_);
    if (success) {
        ScanParticleFiles(); // 保存直後にファイル一覧を更新
    }
    return success;
}

bool GPUParticleEditorContext::LoadSystem(const std::string& filePath) {
    if (!system_) return false;
    if (system_->LoadFromFile(filePath)) {
        system_->Pause(); // ロード直後は停止状態
        system_->SetCurrentTime(0.0f);
        currentFilePath_ = filePath;
        selectedEmitterIndex_ = 0;
        ClearUndoRedo();
        return true;
    }
    return false;
}

void GPUParticleEditorContext::PushUndoState(const std::string& /*desc*/) {
    if (!system_) return;
    undoStack_.push_back(system_->GetData());
    if (undoStack_.size() > 32) {
        undoStack_.erase(undoStack_.begin());
    }
    redoStack_.clear();
}

void GPUParticleEditorContext::PerformUndo() {
    if (undoStack_.empty() || !system_) return;
    redoStack_.push_back(system_->GetData());
    GPUParticleSystemData prev = undoStack_.back();
    undoStack_.pop_back();
    system_->SetData(prev);
    if (selectedEmitterIndex_ >= static_cast<int>(system_->GetEmitterCount())) {
        selectedEmitterIndex_ = (std::max)(0, static_cast<int>(system_->GetEmitterCount()) - 1);
    }
}

void GPUParticleEditorContext::PerformRedo() {
    if (redoStack_.empty() || !system_) return;
    undoStack_.push_back(system_->GetData());
    GPUParticleSystemData next = redoStack_.back();
    redoStack_.pop_back();
    system_->SetData(next);
    if (selectedEmitterIndex_ >= static_cast<int>(system_->GetEmitterCount())) {
        selectedEmitterIndex_ = (std::max)(0, static_cast<int>(system_->GetEmitterCount()) - 1);
    }
}

void GPUParticleEditorContext::ClearUndoRedo() {
    undoStack_.clear();
    redoStack_.clear();
}

void GPUParticleEditorContext::ScanAvailableAssets() {
    availableModels_.clear();
    availableTextures_.clear();
    availableParticleFiles_.clear();

    namespace fs = std::filesystem;

    // 基本モデルのフォールバック
    availableModels_.push_back("resources/Object/School/sphere/sphere.obj");
    availableModels_.push_back("resources/Object/School/cube/cube.obj");

    // 基本テクスチャのフォールバック
    availableTextures_.push_back("white");
    availableTextures_.push_back("resources/Sprite/School/circle.png");
    availableTextures_.push_back("resources/Sprite/School/circle2.png");

    try {
        if (fs::exists("resources")) {
            for (const auto& entry : fs::recursive_directory_iterator("resources")) {
                if (!entry.is_regular_file()) continue;
                std::string ext = entry.path().extension().string();
                std::string pathStr = entry.path().generic_string();

                if (ext == ".obj" || ext == ".gltf") {
                    if (std::find(availableModels_.begin(), availableModels_.end(), pathStr) == availableModels_.end()) {
                        availableModels_.push_back(pathStr);
                    }
                } else if (ext == ".png" || ext == ".jpg" || ext == ".tga" || ext == ".dds") {
                    if (std::find(availableTextures_.begin(), availableTextures_.end(), pathStr) == availableTextures_.end()) {
                        availableTextures_.push_back(pathStr);
                    }
                }
            }
        }
    } catch (...) {
        // スキャン例外は無視してフォールバックを維持
    }

    ScanParticleFiles();
}

void GPUParticleEditorContext::ScanParticleFiles() {
    availableParticleFiles_.clear();
    namespace fs = std::filesystem;

    const std::string particleDir = "resources/json/shared/Particle";
    try {
        if (fs::exists(particleDir)) {
            for (const auto& entry : fs::directory_iterator(particleDir)) {
                if (entry.is_regular_file() && entry.path().extension() == ".json") {
                    std::string pathStr = entry.path().generic_string();
                    availableParticleFiles_.push_back(pathStr);
                }
            }
        }
        if (fs::exists("resources")) {
            for (const auto& entry : fs::recursive_directory_iterator("resources")) {
                if (!entry.is_regular_file()) continue;
                if (entry.path().extension() == ".json") {
                    std::string pathStr = entry.path().generic_string();
                    if (pathStr.find("Particle") != std::string::npos || pathStr.find("particle") != std::string::npos) {
                        if (std::find(availableParticleFiles_.begin(), availableParticleFiles_.end(), pathStr) == availableParticleFiles_.end()) {
                            availableParticleFiles_.push_back(pathStr);
                        }
                    }
                }
            }
        }
    } catch (...) {}
    std::sort(availableParticleFiles_.begin(), availableParticleFiles_.end());
}
#endif
