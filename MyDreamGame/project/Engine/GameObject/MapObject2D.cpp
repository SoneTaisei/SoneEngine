#include "MapObject2D.h"
#include "../../Project/Game2D/Blocks/BaseBlock.h"
#include "../../Project/Game2D/Blocks/BaseBlock/GoalBlock.h"
#include "../../Project/Game2D/Blocks/BaseBlock/NormalBlock.h"
#include "../../Project/Game2D/Blocks/BaseBlock/DeathBlock.h"
#include "../../Project/Game2D/Blocks/BaseBlock/OneWayBlock.h"
#include "../../Project/Game2D/MapChip2D.h"
#include "Component/TransformComponent.h"
#ifdef USE_IMGUI
#include <imgui.h>
#endif

void MapObject2D::SetupDefaultProperties() {
    properties_.clear();
}

void MapObject2D::DisplayImGui() {
#ifdef USE_IMGUI
    char nameBuf[256];
    strcpy_s(nameBuf, sizeof(nameBuf), name_.c_str());
    if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) {
        name_ = nameBuf;
    }
    
    const char* types[] = { "NormalBlock", "DeathBlock", "GoalBlock", "OneWayBlock" };
    int currentType = -1;
    for (int i = 0; i < IM_ARRAYSIZE(types); i++) {
        if (type_ == types[i]) {
            currentType = i;
            break;
        }
    }
    
    if (ImGui::Combo("Type", &currentType, types, IM_ARRAYSIZE(types))) {
        type_ = types[currentType];
        SetupDefaultProperties();
    }
    
    ImGui::DragFloat3("Position", &position_.x, 0.1f);
    ImGui::DragFloat3("Scale", &scale_.x, 0.1f);
    
    ImGui::Separator();
    ImGui::Text("Properties");
    
    // jsonの中身をImGuiで編集可能にする
    for (auto& [key, value] : properties_.items()) {
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
    blockLogic_.reset();
    
    // 仮のグリッド座標として0,0を渡す（自由座標で上書きするため）
    if (type_ == "NormalBlock") blockLogic_ = std::make_shared<NormalBlock>(map, 0, 0);
    else if (type_ == "DeathBlock") blockLogic_ = std::make_shared<DeathBlock>(map, 0, 0);
    else if (type_ == "GoalBlock") blockLogic_ = std::make_shared<GoalBlock>(map, 0, 0);
    else if (type_ == "OneWayBlock") blockLogic_ = std::make_shared<OneWayBlock>(map, 0, 0);
    
    if (blockLogic_) {
        // 幅・高さはスケールとして渡す
        blockLogic_->Initialize(device, boxPrimitive, position_.x, position_.y, scale_.x, scale_.y);
        if (auto* go = blockLogic_->GetGameObject()) {
            go->SetName(name_);
        }
        
        // JSONプロパティを渡す処理
        blockLogic_->SetProperties(properties_);
    }
}

void MapObject2D::Update() {
    if (blockLogic_) {
        if (auto* go = blockLogic_->GetGameObject()) {
            if (auto* tc = go->GetComponent<TransformComponent>()) {
                tc->SetPosition(position_);
                tc->SetScale(scale_);
            }
        }
        blockLogic_->Update();
    }
}

void MapObject2D::Draw() {
    if (blockLogic_) {
        blockLogic_->Draw();
    }
}

void MapObject2D::LoadFromJson(const nlohmann::json& j) {
    if (j.contains("name")) name_ = j["name"];
    if (j.contains("type")) type_ = j["type"];
    if (j.contains("position")) {
        position_.x = j["position"]["x"];
        position_.y = j["position"]["y"];
        position_.z = j["position"]["z"];
    }
    if (j.contains("scale")) {
        scale_.x = j["scale"]["x"];
        scale_.y = j["scale"]["y"];
        scale_.z = j["scale"]["z"];
    }
    if (j.contains("properties")) properties_ = j["properties"];
}

nlohmann::json MapObject2D::SaveToJson() const {
    nlohmann::json j;
    j["name"] = name_;
    j["type"] = type_;
    j["position"] = {{"x", position_.x}, {"y", position_.y}, {"z", position_.z}};
    j["scale"] = {{"x", scale_.x}, {"y", scale_.y}, {"z", scale_.z}};
    j["properties"] = properties_;
    return j;
}
