#include "MapObject2D.h"
#include "../../Project/Game2D/Blocks/BaseBlock.h"
#include "../../Project/Game2D/Blocks/GoalBlock.h"
#include "../../Project/Game2D/Blocks/NormalBlock.h"
#include "../../Project/Game2D/Blocks/DeathBlock.h"
#include "../../Project/Game2D/Blocks/OneWayBlock.h"
#include "../../Project/Game2D/MapChip2D.h"
#ifdef USE_IMGUI
#include <imgui.h>
#endif

void MapObject2D::SetupDefaultProperties() {
    properties.clear();
}

void MapObject2D::DisplayImGui() {
#ifdef USE_IMGUI
    char nameBuf[256];
    strcpy_s(nameBuf, sizeof(nameBuf), name.c_str());
    if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) {
        name = nameBuf;
    }
    
    const char* types[] = { "NormalBlock", "DeathBlock", "GoalBlock", "OneWayBlock" };
    int currentType = -1;
    for (int i = 0; i < IM_ARRAYSIZE(types); i++) {
        if (type == types[i]) {
            currentType = i;
            break;
        }
    }
    
    if (ImGui::Combo("Type", &currentType, types, IM_ARRAYSIZE(types))) {
        type = types[currentType];
        SetupDefaultProperties();
    }
    
    ImGui::DragFloat3("Position", &position.x, 0.1f);
    ImGui::DragFloat3("Scale", &scale.x, 0.1f);
    
    ImGui::Separator();
    ImGui::Text("Properties");
    
    // jsonの中身をImGuiで編集可能にする
    for (auto& [key, value] : properties.items()) {
        if (value.is_number_float()) {
            float v = value.get<float>();
            if (ImGui::DragFloat(key.c_str(), &v, 0.1f)) {
                value = v;
            }
        } else if (value.is_number_integer()) {
            int v = value.get<int>();
            if (ImGui::DragInt(key.c_str(), &v, 1)) {
                value = v;
            }
        } else if (value.is_boolean()) {
            bool v = value.get<bool>();
            if (ImGui::Checkbox(key.c_str(), &v)) {
                value = v;
            }
        } else if (value.is_string()) {
            std::string v = value.get<std::string>();
            char buf[256];
            strcpy_s(buf, sizeof(buf), v.c_str());
            if (ImGui::InputText(key.c_str(), buf, sizeof(buf))) {
                value = buf;
            }
        }
    }
#endif
}

void MapObject2D::InitializeLogic(MapChip2D* map, ID3D12Device* device, Primitive* boxPrimitive) {
    // 古いロジックを破棄
    blockLogic.reset();
    
    // 仮のグリッド座標として0,0を渡す（自由座標で上書きするため）
    if (type == "NormalBlock") blockLogic = std::make_shared<NormalBlock>(map, 0, 0);
    else if (type == "DeathBlock") blockLogic = std::make_shared<DeathBlock>(map, 0, 0);
    else if (type == "GoalBlock") blockLogic = std::make_shared<GoalBlock>(map, 0, 0);
    else if (type == "OneWayBlock") blockLogic = std::make_shared<OneWayBlock>(map, 0, 0);
    
    if (blockLogic) {
        // 幅・高さはスケールとして渡す
        blockLogic->Initialize(device, boxPrimitive, position.x, position.y, scale.x, scale.y);
        if (blockLogic->GetPrimitive()) {
            blockLogic->GetPrimitive()->SetName(name);
        }
        
        // JSONプロパティを渡す処理
        blockLogic->SetProperties(properties);
    }
}

void MapObject2D::Update() {
    if (blockLogic) {
        // 自由配置なので毎フレームTransformをPrimitiveに同期させる？
        // 実際にはブロック側のUpdateで処理されるが、念のため
        if (auto* prim = blockLogic->GetPrimitive()) {
            prim->SetTranslation(position);
            prim->SetScale(scale);
        }
        blockLogic->Update();
    }
}

void MapObject2D::Draw(ID3D12GraphicsCommandList* commandList) {
    if (blockLogic) {
        blockLogic->Draw(commandList);
    }
}

void MapObject2D::LoadFromJson(const nlohmann::json& j) {
    if (j.contains("name")) name = j["name"];
    if (j.contains("type")) type = j["type"];
    if (j.contains("position")) {
        position.x = j["position"]["x"];
        position.y = j["position"]["y"];
        position.z = j["position"]["z"];
    }
    if (j.contains("scale")) {
        scale.x = j["scale"]["x"];
        scale.y = j["scale"]["y"];
        scale.z = j["scale"]["z"];
    }
    if (j.contains("properties")) properties = j["properties"];
}

nlohmann::json MapObject2D::SaveToJson() const {
    nlohmann::json j;
    j["name"] = name;
    j["type"] = type;
    j["position"] = {{"x", position.x}, {"y", position.y}, {"z", position.z}};
    j["scale"] = {{"x", scale.x}, {"y", scale.y}, {"z", scale.z}};
    j["properties"] = properties;
    return j;
}
