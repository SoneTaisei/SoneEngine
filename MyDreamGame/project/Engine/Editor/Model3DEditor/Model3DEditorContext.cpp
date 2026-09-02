#ifdef USE_IMGUI
#include "Model3DEditorContext.h"
#include "Renderer/DirectXCommon/DirectXCommon.h"
#include "Core/Utility/TransformFunctions.h"
#include "Graphics/CameraManager.h"
#include "Core/Utility/UtilityFunctions.h"
#include "Resource/Primitive/PrimitiveManager.h"
#include "GameObject/PrimitiveObject.h"
#include <fstream>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <unordered_map>

Model3DEditorContext::Model3DEditorContext() {
}

Model3DEditorContext::~Model3DEditorContext() = default;

void Model3DEditorContext::Initialize(ID3D12Device* device) {
    device_ = device;
    if (device_) {
        auto* boxPrim = PrimitiveManager::GetInstance()->GetPrimitive(PrimitiveType::Box);
        if (boxPrim) {
            gridFloorObj_ = std::make_unique<PrimitiveObject>();
            gridFloorObj_->Initialize(device_, boxPrim);
            gridFloorObj_->SetName("3DModelEditorGridFloor");
            gridFloorObj_->SetTranslation({ 0.0f, -0.01f, 0.0f });
            gridFloorObj_->SetScale({ 4000.0f, 0.02f, 4000.0f });
            gridFloorObj_->SetIsDoubleSided(true);
            gridFloorObj_->GetMaterial().lightingType = 0; // Unlit
            gridFloorObj_->GetMaterial().color = { 0.0f, 0.0f, 0.0f, 0.0f }; // Transparent floor, only grid lines
            gridFloorObj_->GetMaterial().enableBoxMapping = 2.0f; // Procedural 3D Grid
            gridFloorObj_->Update();
        }
    }
    // Try auto-loading if default file exists
    if (std::filesystem::exists(currentFilePath_)) {
        LoadFromFile(currentFilePath_);
    }
}

void Model3DEditorContext::Update() {
    currentFrame_++;
    if (gridFloorObj_) {
        // Keep grid floor centered at camera XZ position for infinite extent
        Vector3 camPos = CameraManager::GetInstance()->GetCameraPos();
        gridFloorObj_->SetTranslation({ camPos.x, -0.01f, camPos.z });
        gridFloorObj_->Update();
    }
    for (auto& obj : objects_) {
        if (obj) {
            obj->Update();
        }
    }
}

void Model3DEditorContext::Draw() {
    // 1. 3D グリッド床描画 (Zバッファ有効の3D描画のためオブジェクトに貫通・最前面表示されない)
    if (gridFloorObj_) {
        gridFloorObj_->Draw();
    }

    // 2. 配置した3Dモデルの描画
    for (auto& obj : objects_) {
        if (obj) {
            obj->Draw();
        }
    }
}

Model3DEditorContext::Model3DEditorSnapshot Model3DEditorContext::CreateSnapshot() const {
    Model3DEditorSnapshot snap;
    snap.selectedId = selectedObject_ ? selectedObject_->GetId() : 0;
    for (const auto& obj : objects_) {
        if (!obj) continue;
        PlacedObjectSnapshot os;
        os.id = obj->GetId();
        os.name = obj->GetName();
        os.modelDirectory = obj->GetModelDirectory();
        os.modelFileName = obj->GetModelFileName();
        os.translation = obj->GetTranslation();
        os.rotation = obj->GetRotation();
        os.scale = obj->GetScale();
        os.color = obj->GetColor();
        os.doubleSided = obj->IsDoubleSided();
        os.texturePath = obj->GetTexturePath();
        snap.objects.push_back(os);
    }
    return snap;
}

void Model3DEditorContext::RestoreSnapshot(const Model3DEditorSnapshot& snap) {
    if (!device_) {
        device_ = DirectXCommon::GetInstance()->GetDevice();
    }

    // 既存オブジェクトをIDマップに退避
    std::unordered_map<uint64_t, std::unique_ptr<PlacedObject3D>> existingMap;
    for (auto& obj : objects_) {
        if (obj) {
            uint64_t id = obj->GetId();
            existingMap[id] = std::move(obj);
        }
    }
    objects_.clear();
    selectedObject_ = nullptr;

    for (const auto& os : snap.objects) {
        auto it = existingMap.find(os.id);
        if (it != existingMap.end() && it->second) {
            // 既存オブジェクトを再利用し、座標・回転・スケール等のプロパティをその場で復元
            auto obj = std::move(it->second);
            obj->SetName(os.name);
            obj->SetTranslation(os.translation);
            obj->SetRotation(os.rotation);
            obj->SetScale(os.scale);
            obj->SetColor(os.color);
            obj->SetDoubleSided(os.doubleSided);
            if (obj->GetTexturePath() != os.texturePath) {
                obj->SetTexture(os.texturePath);
            }
            obj->Update();
            if (os.id == snap.selectedId) {
                selectedObject_ = obj.get();
            }
            objects_.push_back(std::move(obj));
        } else {
            // 新規作成が必要なオブジェクト
            auto obj = std::make_unique<PlacedObject3D>(os.name, os.modelDirectory, os.modelFileName);
            obj->SetId(os.id);
            obj->SetTranslation(os.translation);
            obj->SetRotation(os.rotation);
            obj->SetScale(os.scale);
            obj->SetColor(os.color);
            obj->SetDoubleSided(os.doubleSided);
            if (!os.texturePath.empty()) {
                obj->SetTexture(os.texturePath);
            }
            if (device_) {
                obj->Initialize(device_);
            }
            obj->Update();
            if (os.id == snap.selectedId) {
                selectedObject_ = obj.get();
            }
            objects_.push_back(std::move(obj));
        }
    }

    // もしselectedObject_がnullptrでかつオブジェクトが存在し、元の選択があった場合は先頭などをフォールバック
    if (!selectedObject_ && !objects_.empty() && snap.selectedId != 0) {
        selectedObject_ = objects_[0].get();
    }
}

void Model3DEditorContext::PushUndoState() {
    undoStack_.push_back(CreateSnapshot());
    if (undoStack_.size() > 100) {
        undoStack_.erase(undoStack_.begin());
    }
    redoStack_.clear();
}

void Model3DEditorContext::PushSnapshotToUndo(const Model3DEditorSnapshot& snapshot) {
    undoStack_.push_back(snapshot);
    if (undoStack_.size() > 100) {
        undoStack_.erase(undoStack_.begin());
    }
    redoStack_.clear();
}

void Model3DEditorContext::Undo() {
    if (undoStack_.empty()) return;
    if (lastUndoRedoFrame_ == currentFrame_ && currentFrame_ > 0) return;
    lastUndoRedoFrame_ = currentFrame_;

    redoStack_.push_back(CreateSnapshot());
    auto snap = undoStack_.back();
    undoStack_.pop_back();
    RestoreSnapshot(snap);
}

void Model3DEditorContext::Redo() {
    if (redoStack_.empty()) return;
    if (lastUndoRedoFrame_ == currentFrame_ && currentFrame_ > 0) return;
    lastUndoRedoFrame_ = currentFrame_;

    undoStack_.push_back(CreateSnapshot());
    auto snap = redoStack_.back();
    redoStack_.pop_back();
    RestoreSnapshot(snap);
}

PlacedObject3D* Model3DEditorContext::AddObject(const std::string& name, const std::string& modelDir, const std::string& modelFileName, const Vector3& position) {
    PushUndoState();

    if (!device_) {
        device_ = DirectXCommon::GetInstance()->GetDevice();
    }
    auto newObj = std::make_unique<PlacedObject3D>(name, modelDir, modelFileName);
    newObj->SetTranslation(position);
    if (device_) {
        newObj->Initialize(device_);
    }
    PlacedObject3D* ptr = newObj.get();
    objects_.push_back(std::move(newObj));
    selectedObject_ = ptr;
    return ptr;
}

void Model3DEditorContext::RemoveObject(PlacedObject3D* target) {
    if (!target) return;
    PushUndoState();

    if (selectedObject_ == target) {
        selectedObject_ = nullptr;
    }
    objects_.erase(
        std::remove_if(objects_.begin(), objects_.end(),
            [target](const std::unique_ptr<PlacedObject3D>& obj) {
                return obj.get() == target;
            }),
        objects_.end()
    );
}

PlacedObject3D* Model3DEditorContext::DuplicateObject(PlacedObject3D* target) {
    if (!target || !device_) return nullptr;
    PushUndoState();

    nlohmann::json j = target->ToJson();
    auto dup = std::make_unique<PlacedObject3D>();
    dup->FromJson(j, device_);
    dup->SetName(target->GetName() + "_Copy");
    // Offset slightly
    Vector3 pos = dup->GetTranslation();
    pos.x += 1.0f;
    dup->SetTranslation(pos);
    dup->Update();

    PlacedObject3D* ptr = dup.get();
    objects_.push_back(std::move(dup));
    selectedObject_ = ptr;
    return ptr;
}

void Model3DEditorContext::ClearObjects() {
    if (!objects_.empty()) {
        PushUndoState();
    }
    selectedObject_ = nullptr;
    objects_.clear();
}

bool Model3DEditorContext::SaveToFile(const std::string& filePath) {
    std::string path = filePath.empty() ? currentFilePath_ : filePath;
    try {
        std::filesystem::path p(path);
        if (p.has_parent_path()) {
            std::filesystem::create_directories(p.parent_path());
        }

        nlohmann::json root = nlohmann::json::array();
        for (const auto& obj : objects_) {
            if (obj) {
                root.push_back(obj->ToJson());
            }
        }

        std::ofstream ofs(path);
        if (!ofs.is_open()) return false;
        ofs << root.dump(4);
        currentFilePath_ = path;
        return true;
    } catch (...) {
        return false;
    }
}

bool Model3DEditorContext::LoadFromFile(const std::string& filePath) {
    std::string path = filePath.empty() ? currentFilePath_ : filePath;
    if (!std::filesystem::exists(path)) return false;

    try {
        std::ifstream ifs(path);
        if (!ifs.is_open()) return false;
        nlohmann::json root = nlohmann::json::parse(ifs);

        if (!root.is_array()) return false;

        ClearObjects();

        for (const auto& item : root) {
            auto obj = std::make_unique<PlacedObject3D>();
            if (obj->FromJson(item, device_)) {
                objects_.push_back(std::move(obj));
            }
        }

        currentFilePath_ = path;
        return true;
    } catch (...) {
        return false;
    }
}

PlacedObject3D* Model3DEditorContext::PickObject(const Vector3& rayOrigin, const Vector3& rayDir, float& outDist) {
    PlacedObject3D* closestObj = nullptr;
    float closestDist = 1e9f;

    for (auto& obj : objects_) {
        if (!obj) continue;
        float t = 0.0f;
        if (obj->IntersectRay(rayOrigin, rayDir, t)) {
            if (t > 0.0f && t < closestDist) {
                closestDist = t;
                closestObj = obj.get();
            }
        }
    }

    outDist = closestDist;
    return closestObj;
}
#endif
