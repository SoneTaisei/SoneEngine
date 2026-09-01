#ifdef USE_IMGUI
#include "Model3DEditorContext.h"
#include "Renderer/DirectXCommon/DirectXCommon.h"
#include "Core/Utility/TransformFunctions.h"
#include "Graphics/CameraManager.h"
#include "Core/Utility/UtilityFunctions.h"
#include <fstream>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <unordered_map>

Model3DEditorContext::Model3DEditorContext() {
}

void Model3DEditorContext::Initialize(ID3D12Device* device) {
    device_ = device;
    if (device_) {
        CreateGridBuffers(device_);
    }
    // Try auto-loading if default file exists
    if (std::filesystem::exists(currentFilePath_)) {
        LoadFromFile(currentFilePath_);
    }
}

void Model3DEditorContext::CreateGridBuffers(ID3D12Device* device) {
    if (!device) return;

    std::vector<VertexData> vertices;
    std::vector<uint32_t> indices;

    const float extent = 100.0f;
    const float step = 1.0f;
    const float halfW = 0.012f;

    auto addLineQuad = [&](float x1, float z1, float x2, float z2, float w) {
        float dx = x2 - x1;
        float dz = z2 - z1;
        float len = std::sqrt(dx * dx + dz * dz);
        if (len < 1e-4f) return;
        float nx = -dz / len * w;
        float nz = dx / len * w;

        uint32_t startIdx = static_cast<uint32_t>(vertices.size());

        VertexData v0{ { x1 + nx, 0.001f, z1 + nz, 1.0f }, { 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f } };
        VertexData v1{ { x1 - nx, 0.001f, z1 - nz, 1.0f }, { 1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f } };
        VertexData v2{ { x2 - nx, 0.001f, z2 - nz, 1.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f, 0.0f } };
        VertexData v3{ { x2 + nx, 0.001f, z2 + nz, 1.0f }, { 0.0f, 1.0f }, { 0.0f, 1.0f, 0.0f } };

        vertices.push_back(v0);
        vertices.push_back(v1);
        vertices.push_back(v2);
        vertices.push_back(v3);

        indices.push_back(startIdx + 0);
        indices.push_back(startIdx + 1);
        indices.push_back(startIdx + 2);
        indices.push_back(startIdx + 0);
        indices.push_back(startIdx + 2);
        indices.push_back(startIdx + 3);
    };

    // 通常グリッド線 (X平行線 & Z平行線)
    for (float x = -extent; x <= extent; x += step) {
        if (std::abs(x) < 0.001f) continue;
        addLineQuad(x, -extent, x, extent, halfW);
    }
    for (float z = -extent; z <= extent; z += step) {
        if (std::abs(z) < 0.001f) continue;
        addLineQuad(-extent, z, extent, z, halfW);
    }

    // 主軸線 (X軸 & Z軸)
    addLineQuad(-extent, 0.0f, extent, 0.0f, halfW * 2.0f);
    addLineQuad(0.0f, -extent, 0.0f, extent, halfW * 2.0f);

    gridIndexCount_ = static_cast<uint32_t>(indices.size());

    // Vertex Buffer
    gridVertexResource_ = CreateBufferResource(device, sizeof(VertexData) * vertices.size());
    VertexData* mappedVertices = nullptr;
    gridVertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedVertices));
    std::memcpy(mappedVertices, vertices.data(), sizeof(VertexData) * vertices.size());
    gridVertexResource_->Unmap(0, nullptr);

    gridVertexBufferView_.BufferLocation = gridVertexResource_->GetGPUVirtualAddress();
    gridVertexBufferView_.SizeInBytes = static_cast<UINT>(sizeof(VertexData) * vertices.size());
    gridVertexBufferView_.StrideInBytes = sizeof(VertexData);

    // Index Buffer
    gridIndexResource_ = CreateBufferResource(device, sizeof(uint32_t) * indices.size());
    uint32_t* mappedIndices = nullptr;
    gridIndexResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedIndices));
    std::memcpy(mappedIndices, indices.data(), sizeof(uint32_t) * indices.size());
    gridIndexResource_->Unmap(0, nullptr);

    gridIndexBufferView_.BufferLocation = gridIndexResource_->GetGPUVirtualAddress();
    gridIndexBufferView_.SizeInBytes = static_cast<UINT>(sizeof(uint32_t) * indices.size());
    gridIndexBufferView_.Format = DXGI_FORMAT_R32_UINT;

    // Transform & Material
    gridTransformResource_ = CreateBufferResource(device, (sizeof(TransformMatrix) + 255) & ~255u);
    gridTransformResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedGridTransform_));

    gridMaterialResource_ = CreateBufferResource(device, (sizeof(Material) + 255) & ~255u);
    gridMaterialResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedGridMaterial_));

    mappedGridMaterial_->color = { 0.35f, 0.35f, 0.38f, 0.75f };
    mappedGridMaterial_->lightingType = 0;
    mappedGridMaterial_->uvTransform = TransformFunctions::MakeIdentity4x4();
    mappedGridMaterial_->shininess = 1.0f;
    mappedGridMaterial_->enableEnvironmentMap = 0;
}

void Model3DEditorContext::DrawGrid3D() {
    if (!device_ || gridIndexCount_ == 0 || !mappedGridTransform_ || !mappedGridMaterial_) return;

    DirectXCommon* dxCommon = DirectXCommon::GetInstance();
    if (!dxCommon) return;

    auto commandList = dxCommon->GetCommandList();
    if (!commandList) return;

    CameraManager* cameraMgr = CameraManager::GetInstance();
    Matrix4x4 viewMatrix = cameraMgr->GetViewMatrix();
    Matrix4x4 projectionMatrix = cameraMgr->GetProjectionMatrix();

    Matrix4x4 identity = TransformFunctions::MakeIdentity4x4();
    mappedGridTransform_->World = identity;
    mappedGridTransform_->WorldInverseTranspose = identity;
    mappedGridTransform_->WVP = TransformFunctions::Multiply(TransformFunctions::Multiply(identity, viewMatrix), projectionMatrix);

    *mappedGridMaterial_ = *mappedGridMaterial_;

    commandList->SetGraphicsRootSignature(dxCommon->GetRootSignature());
    commandList->SetPipelineState(dxCommon->GetGraphicsPipelineStateTransparent());

    commandList->SetGraphicsRootConstantBufferView(1, gridTransformResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(0, gridMaterialResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(3, CameraManager::GetInstance()->GetCameraGPUAddress());

    commandList->IASetVertexBuffers(0, 1, &gridVertexBufferView_);
    commandList->IASetIndexBuffer(&gridIndexBufferView_);
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    commandList->DrawIndexedInstanced(gridIndexCount_, 1, 0, 0, 0);
}

void Model3DEditorContext::Update() {
    currentFrame_++;
    for (auto& obj : objects_) {
        if (obj) {
            obj->Update();
        }
    }
}

void Model3DEditorContext::Draw() {
    // 1. 3D グリッド描画 (Zバッファ有効の3D描画のためオブジェクトに貫通・最前面表示されない)
    DrawGrid3D();

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
