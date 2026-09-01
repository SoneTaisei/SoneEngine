#ifdef USE_IMGUI
#include "MapEditorInspector.h"
#include "MapEditorContext.h"
#include "Scene/SceneManager.h"
#include "Scene/IScene.h"
#include "Game2D/MapChip2D.h"
#include "Game2D/Blocks/BlockFactory.h"
#include "Game2D/Blocks/BaseBlock.h"
#include <string>
#include <algorithm>
#include <vector>

MapEditorInspector::MapEditorInspector(MapEditorContext* context)
    : context_(context) {
}

bool MapEditorInspector::Draw(SceneManager* sceneManager) {
    if (!context_) return false;

    IScene* activeScene = sceneManager ? sceneManager->GetCurrentScene() : nullptr;
    if (!activeScene) return false;

    MapChip2D* mapChip = activeScene->GetMapChip();
    if (!mapChip) return false;

    int selectedTool = context_->GetSelectedTool();
    if (selectedTool < 100 && (selectedTool < 1 || selectedTool > 12)) {
        return false;
    }

    MapChip2D::CustomBlockDef* targetDef = nullptr;
    bool isTemplate = false;
    bool changed = false;

    if (selectedTool >= 100) {
        auto& palette = mapChip->GetCustomPalette();
        for (auto& def : palette) {
            if (def.id == selectedTool) {
                targetDef = &def;
                break;
            }
        }
    } else {
        auto& templates = mapChip->GetTemplatePalette();
        for (auto& def : templates) {
            if (def.id == selectedTool) {
                targetDef = &def;
                isTemplate = true;
                break;
            }
        }
    }

    if (!targetDef) return false;

    if (isTemplate) {
        ImGui::Text("Template Settings (ID: %d)", targetDef->id);
    } else {
        ImGui::Text("Custom Block Settings (ID: %d)", targetDef->id);
    }

    char nameBuf[256];
    strcpy_s(nameBuf, sizeof(nameBuf), targetDef->name.c_str());
    if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) {
        targetDef->name = nameBuf;
        changed = true;
    }

    // 登録済みの全ブロック型名リストを取得
    std::vector<std::string> availableTypes = BlockFactory::GetInstance().GetAvailableTypes();
    for (const auto& t : mapChip->GetTemplatePalette()) {
        if (!t.type.empty() && std::find(availableTypes.begin(), availableTypes.end(), t.type) == availableTypes.end()) {
            availableTypes.push_back(t.type);
        }
    }

    if (ImGui::BeginCombo("種類 (Type)", targetDef->type.c_str())) {
        for (const auto& typeName : availableTypes) {
            bool isSelected = (targetDef->type == typeName);
            if (ImGui::Selectable(typeName.c_str(), isSelected)) {
                targetDef->type = typeName;
                changed = true;
                bool foundTemplate = false;
                for (const auto& t : mapChip->GetTemplatePalette()) {
                    if (t.type == targetDef->type) {
                        targetDef->properties = t.properties;
                        foundTemplate = true;
                        break;
                    }
                }
                if (!foundTemplate) {
                    targetDef->properties = nlohmann::json::object();
                }
            }
            if (isSelected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    float col[4] = { targetDef->color.x, targetDef->color.y, targetDef->color.z, targetDef->color.w };
    if (ImGui::ColorEdit4("色 (Color)", col)) {
        targetDef->color = { col[0], col[1], col[2], col[3] };
        changed = true;
    }

    float scale[3] = { targetDef->scale.x, targetDef->scale.y, targetDef->scale.z };
    if (ImGui::DragFloat3("スケール (Scale)", scale, 0.01f)) {
        targetDef->scale = { scale[0], scale[1], scale[2] };
        changed = true;
    }

    const auto& availableModels = context_->GetAvailableModels();
    if (ImGui::BeginCombo("モデル (Model)", targetDef->modelName.empty() ? "なし (None)" : targetDef->modelName.c_str())) {
        bool isNoneSelected = targetDef->modelName.empty();
        if (ImGui::Selectable("なし (None)", isNoneSelected)) {
            targetDef->modelName = "";
            changed = true;
        }
        if (isNoneSelected) {
            ImGui::SetItemDefaultFocus();
        }
        for (const auto& modelPath : availableModels) {
            bool isSelected = (targetDef->modelName == modelPath);
            if (ImGui::Selectable(modelPath.c_str(), isSelected)) {
                targetDef->modelName = modelPath;
                changed = true;
            }
            if (isSelected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    const auto& availableTextures = context_->GetAvailableTextures();
    if (ImGui::BeginCombo("テクスチャ (Texture)", targetDef->textureName.empty() ? "なし (None)" : targetDef->textureName.c_str())) {
        bool isTexNoneSelected = targetDef->textureName.empty();
        if (ImGui::Selectable("なし (None)", isTexNoneSelected)) {
            targetDef->textureName = "";
            changed = true;
        }
        if (isTexNoneSelected) {
            ImGui::SetItemDefaultFocus();
        }
        for (const auto& texPath : availableTextures) {
            bool isSelected = (targetDef->textureName == texPath);
            if (ImGui::Selectable(texPath.c_str(), isSelected)) {
                targetDef->textureName = texPath;
                changed = true;
            }
            if (isSelected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    ImGui::Separator();
    ImGui::Text("プロパティ:");
    auto getJpKey = [](const std::string& k) {
        if (k == "speed") return std::string("スピード (speed)");
        if (k == "speedForward") return std::string("往路の速さ (speedForward)");
        if (k == "speedBackward") return std::string("復路の速さ (speedBackward)");
        if (k == "waitTime") return std::string("待機時間 (waitTime)");
        if (k == "acceleration") return std::string("加速度 (acceleration)");
        if (k == "maxSpeedForward") return std::string("往路の最高速度 (maxSpeedForward)");
        if (k == "maxSpeedBackward") return std::string("復路の最高速度 (maxSpeedBackward)");
        if (k == "maxSpeed") return std::string("最高速度 (maxSpeed)");
        if (k == "direction") return std::string("方向 (direction)");
        if (k == "range") return std::string("移動距離 (range)");
        if (k == "jumpVelocityVertical") return std::string("縦ジャンプ力 (jumpVelocityVertical)");
        if (k == "jumpVelocityHorizontal") return std::string("横ジャンプ力 (jumpVelocityHorizontal)");
        if (k == "moveSpeed") return std::string("移動速度 (moveSpeed)");
        return k;
    };

    for (auto& [key, value] : targetDef->properties.items()) {
        std::string jpKey = getJpKey(key);
        if (value.is_number()) {
            float v = value.get<float>();
            if (ImGui::DragFloat(jpKey.c_str(), &v, 0.1f)) {
                value = v;
                changed = true;
            }
        } else if (value.is_string()) {
            std::string v = value.get<std::string>();
            char buf[256];
            strcpy_s(buf, sizeof(buf), v.c_str());
            if (ImGui::InputText(jpKey.c_str(), buf, sizeof(buf))) {
                value = buf;
                changed = true;
            }
        } else if (value.is_boolean()) {
            bool v = value.get<bool>();
            if (ImGui::Checkbox(jpKey.c_str(), &v)) {
                value = v;
                changed = true;
            }
        }
    }

    // ブロッククラス固有の ImGui UI (DrawImGui) の表示
    if (BlockFactory::GetInstance().HasType(targetDef->type)) {
        ImGui::Separator();
        static std::shared_ptr<BaseBlock> previewBlock = nullptr;
        static std::string lastPreviewType = "";
        if (!previewBlock || lastPreviewType != targetDef->type) {
            previewBlock = BlockFactory::GetInstance().Create(targetDef->type, mapChip, 0, 0);
            lastPreviewType = targetDef->type;
        }
        if (previewBlock) {
            previewBlock->SetProperties(targetDef->properties);
            previewBlock->DrawImGui();
        }
    }

    static bool autoApply = true;
    ImGui::Checkbox("自動適用 (Auto Apply)", &autoApply);
    ImGui::SameLine();
    if (ImGui::Button("デフォルトに戻す (Reset to Default)")) {
        auto& templates = mapChip->GetTemplatePalette();
        for (const auto& t : templates) {
            if (t.type == targetDef->type) {
                targetDef->color = t.color;
                targetDef->scale = t.scale;
                targetDef->modelName = t.modelName;
                targetDef->textureName = t.textureName;
                targetDef->properties = t.properties;
                changed = true;
                break;
            }
        }
    }

    if (changed && autoApply) {
        mapChip->SaveToFile(context_->GetFullFilePath(context_->GetStageFilename()));
    }

    return true;
}
#endif
