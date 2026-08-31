#include "LevelDataLoader.h"
#include "Resource/Model/ModelManager.h"
#include "Core/Utility/LogManager.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <algorithm>
#include <windows.h>

#ifdef USE_IMGUI
#include "../externals/imgui/imgui.h"
#endif

using namespace LevelDataStructs;
namespace fs = std::filesystem;

constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;

// UTF-8 の std::string パスを Windows Unicode API 用の std::wstring に変換する
static std::wstring Utf8ToWide(const std::string& str) {
    if (str.empty()) return L"";
    int count = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    if (count <= 0) return L"";
    std::wstring wstr(count, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &wstr[0], count);
    if (!wstr.empty() && wstr.back() == L'\0') {
        wstr.pop_back();
    }
    return wstr;
}

// 存在確認と補正を行ったファイルパス（std::wstring）を取得するヘルパー
static std::wstring ResolveFilePathW(const std::string& inputPath) {
    std::wstring winput = Utf8ToWide(inputPath);
    if (fs::exists(winput)) {
        return winput;
    }

    std::vector<std::string> candidates = {
        "../tools/" + inputPath,
        "tools/" + inputPath,
        "../tools/blender_addons/" + inputPath,
        "resources/" + inputPath,
        "resources/json/" + inputPath,
        "resources/json/shared/Map/" + inputPath
    };

    if (inputPath.find("TL.json") != std::string::npos || inputPath.find("TL.scene") != std::string::npos) {
        std::string filename = fs::path(inputPath).filename().string();
        candidates.push_back("../tools/" + filename);
        candidates.push_back("tools/" + filename);
        candidates.push_back("./" + filename);
    }

    for (const auto& candidate : candidates) {
        std::wstring wc = Utf8ToWide(candidate);
        if (fs::exists(wc)) {
            return wc;
        }
    }
    return winput;
}

bool LevelDataLoader::LoadFile(const std::string& filePath) {
    std::wstring resolvedW = ResolveFilePathW(filePath);
    fs::path p(resolvedW);
    std::string ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == ".scene") {
        return LoadSceneText(filePath);
    }
    return LoadJSON(filePath);
}

bool LevelDataLoader::LoadJSON(const std::string& filePath) {
    std::wstring resolvedW = ResolveFilePathW(filePath);
    loadedFilePath_ = filePath;
    isLoaded_ = false;
    levelData_ = LevelData();

    std::ifstream file(resolvedW);
    if (!file.is_open()) {
        std::string log = "[LevelDataLoader] Failed to open file: " + filePath;
        LogManager::GetInstance()->AddLog(LogLevel::Error, log);
        return false;
    }

    nlohmann::json deserialized;
    try {
        file >> deserialized;
    } catch (const nlohmann::json::parse_error& e) {
        std::string log = "[LevelDataLoader] JSON parse error in " + filePath + ": " + e.what();
        LogManager::GetInstance()->AddLog(LogLevel::Error, log);
        return false;
    }

    if (!deserialized.is_object() ||
        !deserialized.contains("name") ||
        !deserialized["name"].is_string()) {
        LogManager::GetInstance()->AddLog(LogLevel::Error, "[LevelDataLoader] Invalid level data file (missing or invalid 'name').");
        return false;
    }

    std::string sceneName = deserialized["name"].get<std::string>();
    if (sceneName != "scene") {
        LogManager::GetInstance()->AddLog(LogLevel::Error, "[LevelDataLoader] Header name is not 'scene': " + sceneName);
        return false;
    }

    levelData_.name = sceneName;

    if (deserialized.contains("objects") && deserialized["objects"].is_array()) {
        for (const auto& objectJson : deserialized["objects"]) {
            ObjectData objectData;
            ParseObjectRecursive(objectJson, objectData);
            levelData_.objects.push_back(objectData);
        }
    }

    isLoaded_ = true;
    LogManager::GetInstance()->AddLog(LogLevel::Info, "[LevelDataLoader] Successfully loaded level data: " + filePath);
    return true;
}

bool LevelDataLoader::LoadSceneText(const std::string& filePath) {
    std::wstring resolvedW = ResolveFilePathW(filePath);
    loadedFilePath_ = filePath;
    isLoaded_ = false;
    levelData_ = LevelData();

    std::ifstream file(resolvedW);
    if (!file.is_open()) {
        LogManager::GetInstance()->AddLog(LogLevel::Error, "[LevelDataLoader] Failed to open .scene file: " + filePath);
        return false;
    }

    std::string line;
    if (!std::getline(file, line) || line.find("SCENE") == std::string::npos) {
        LogManager::GetInstance()->AddLog(LogLevel::Error, "[LevelDataLoader] Header is not SCENE in: " + filePath);
        return false;
    }

    levelData_.name = "scene";
    ObjectData currentObj;
    bool inObject = false;

    while (std::getline(file, line)) {
        auto start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        line = line.substr(start);

        std::stringstream ss(line);
        std::string token;
        ss >> token;

        if (token == "MESH" || token == "LIGHT" || token == "CAMERA") {
            currentObj = ObjectData();
            currentObj.type = token;
            currentObj.name = token;
            inObject = true;
        } else if (inObject && token == "T") {
            ss >> currentObj.transform.translation.x >> currentObj.transform.translation.z >> currentObj.transform.translation.y;
        } else if (inObject && token == "R") {
            float rx, ry, rz;
            ss >> rx >> rz >> ry;
            currentObj.transform.rotation = { -rx, -ry, -rz };
        } else if (inObject && token == "S") {
            ss >> currentObj.transform.scaling.x >> currentObj.transform.scaling.z >> currentObj.transform.scaling.y;
        } else if (inObject && token == "N") {
            ss >> currentObj.fileName;
        } else if (inObject && token == "C") {
            currentObj.hasCollider = true;
            ss >> currentObj.collider.type;
        } else if (inObject && token == "END") {
            levelData_.objects.push_back(currentObj);
            inObject = false;
        }
    }

    isLoaded_ = true;
    LogManager::GetInstance()->AddLog(LogLevel::Info, "[LevelDataLoader] Successfully loaded .scene text file: " + filePath);
    return true;
}

void LevelDataLoader::ParseObjectRecursive(const nlohmann::json& objectJson, ObjectData& outObjectData) {
    if (objectJson.contains("type") && objectJson["type"].is_string()) {
        outObjectData.type = objectJson["type"].get<std::string>();
    }
    if (objectJson.contains("name") && objectJson["name"].is_string()) {
        outObjectData.name = objectJson["name"].get<std::string>();
    }

    if (objectJson.contains("transform") && objectJson["transform"].is_object()) {
        const auto& transform = objectJson["transform"];

        if (transform.contains("translation") && transform["translation"].is_array()) {
            outObjectData.transform.translation.x = (float)transform["translation"][0];
            outObjectData.transform.translation.y = (float)transform["translation"][2]; // Blender Z -> Game Y
            outObjectData.transform.translation.z = (float)transform["translation"][1]; // Blender Y -> Game Z
        }

        if (transform.contains("rotation") && transform["rotation"].is_array()) {
            outObjectData.transform.rotation.x = -(float)transform["rotation"][0];
            outObjectData.transform.rotation.y = -(float)transform["rotation"][2]; // Blender Z -> Game Y
            outObjectData.transform.rotation.z = -(float)transform["rotation"][1]; // Blender Y -> Game Z
        }

        if (transform.contains("scaling") && transform["scaling"].is_array()) {
            outObjectData.transform.scaling.x = (float)transform["scaling"][0];
            outObjectData.transform.scaling.y = (float)transform["scaling"][2]; // Blender Z -> Game Y
            outObjectData.transform.scaling.z = (float)transform["scaling"][1]; // Blender Y -> Game Z
        }
    }

    if (objectJson.contains("file_name") && objectJson["file_name"].is_string()) {
        outObjectData.fileName = objectJson["file_name"].get<std::string>();
    }

    if (objectJson.contains("無効オプション")) {
        if (objectJson["無効オプション"].is_boolean()) {
            outObjectData.disabled = objectJson["無効オプション"].get<bool>();
        }
    } else if (objectJson.contains("disabled")) {
        if (objectJson["disabled"].is_boolean()) {
            outObjectData.disabled = objectJson["disabled"].get<bool>();
        }
    }

    if (objectJson.contains("collider") && objectJson["collider"].is_object()) {
        outObjectData.hasCollider = true;
        const auto& col = objectJson["collider"];
        if (col.contains("type") && col["type"].is_string()) {
            outObjectData.collider.type = col["type"].get<std::string>();
        }
    }

    if (objectJson.contains("children") && objectJson["children"].is_array()) {
        for (const auto& childJson : objectJson["children"]) {
            ObjectData childData;
            ParseObjectRecursive(childJson, childData);
            outObjectData.children.push_back(childData);
        }
    }

    if (outObjectData.type == "PlayerSpawn") {
        PlayerSpawnData playerData;
        playerData.translation = outObjectData.transform.translation;
        playerData.rotation = outObjectData.transform.rotation;
        levelData_.players.push_back(playerData);
    } else if (outObjectData.type == "EnemySpawn") {
        EnemySpawnData enemyData;
        enemyData.translation = outObjectData.transform.translation;
        enemyData.rotation = outObjectData.transform.rotation;
        enemyData.fileName = outObjectData.fileName;
        levelData_.enemies.push_back(enemyData);
    }
}

std::vector<std::unique_ptr<Object3D>> LevelDataLoader::CreateObjects(
    ID3D12Device* device,
    ModelCommon* modelCommon,
    const std::string& baseDirectoryPath) {
    
    std::vector<std::unique_ptr<Object3D>> createdObjects;

    if (!isLoaded_) {
        LogManager::GetInstance()->AddLog(LogLevel::Warning, "[LevelDataLoader] Cannot create objects: No level data loaded.");
        return createdObjects;
    }

    for (const auto& objectData : levelData_.objects) {
        CreateObjectRecursive(objectData, device, modelCommon, baseDirectoryPath, createdObjects);
    }

    return createdObjects;
}

// 存在する安全なモデルファイルを取得するヘルパー関数
static Model* GetSafeModel(const std::string& requestedName, const std::string& objName) {
    // 1. ICO球 / Sphere 判定
    if (objName.find("ICO") != std::string::npos || objName.find("球") != std::string::npos || objName.find("Sphere") != std::string::npos) {
        return ModelManager::GetInstance()->GetModel("resources/Object/Original/sphere", "sphere.obj");
    }

    // 2. Cube 判定 (resources/Object/Original/cube/cube.obj を優先使用)
    if (objName.find("Cube") != std::string::npos || objName.find("cube") != std::string::npos || requestedName.find("cube") != std::string::npos || requestedName.find("Cube") != std::string::npos) {
        if (fs::exists(Utf8ToWide("resources/Object/Original/cube/cube.obj"))) {
            return ModelManager::GetInstance()->GetModel("resources/Object/Original/cube", "cube.obj");
        }
    }

    // 3. requestedName (例: "gaikotu", "testFbx") での実ファイル検索
    if (!requestedName.empty()) {
        std::vector<std::pair<std::string, std::string>> checkPaths = {
            {"resources/Object/Original/" + requestedName, "scene.gltf"},
            {"resources/Object/School/" + requestedName, requestedName + ".obj"},
            {"resources/Object/School/" + requestedName, requestedName + ".gltf"},
            {"resources/Object/Original/" + requestedName, requestedName + ".obj"},
            {"resources/Object/Original/" + requestedName, requestedName + ".gltf"}
        };

        for (const auto& pair : checkPaths) {
            std::string fullPath = pair.first + "/" + pair.second;
            if (fs::exists(Utf8ToWide(fullPath))) {
                return ModelManager::GetInstance()->GetModel(pair.first, pair.second);
            }
        }
    }

    // 4. 安全なデフォルトキューブモデル
    if (fs::exists(Utf8ToWide("resources/Object/Original/cube/cube.obj"))) {
        return ModelManager::GetInstance()->GetModel("resources/Object/Original/cube", "cube.obj");
    }
    if (fs::exists(Utf8ToWide("resources/Object/School/multiMesh/multiMesh.obj"))) {
        return ModelManager::GetInstance()->GetModel("resources/Object/School/multiMesh", "multiMesh.obj");
    }

    // 4. 平面モデル
    if (fs::exists(Utf8ToWide("resources/Object/School/plane/plane.obj"))) {
        return ModelManager::GetInstance()->GetModel("resources/Object/School/plane", "plane.obj");
    }

    // 5. 最後の手段
    return ModelManager::GetInstance()->GetModel("resources/Object/Original/sphere", "sphere.obj");
}

void LevelDataLoader::CreateObjectRecursive(
    const ObjectData& objectData,
    ID3D12Device* device,
    ModelCommon* modelCommon,
    const std::string& baseDirectoryPath,
    std::vector<std::unique_ptr<Object3D>>& outObjects) {

    // 有効無効フラグが true (無効) の場合は配置しない (スキップ)
    if (objectData.disabled) {
        return;
    }

    // "MESH" タイプの場合、モデルを取得してオブジェクトを生成
    if (objectData.type == "MESH") {
        Model* model = GetSafeModel(objectData.fileName, objectData.name);

        if (model) {
            auto newObject = std::make_unique<Object3D>();
            newObject->Initialize(device, model);

            // トランスフォームの設定
            newObject->SetTranslation(objectData.transform.translation);
            
            Vector3 radRotation = {
                objectData.transform.rotation.x * kDegToRad,
                objectData.transform.rotation.y * kDegToRad,
                objectData.transform.rotation.z * kDegToRad
            };
            newObject->SetRotation(radRotation);
            newObject->SetScale(objectData.transform.scaling);
            newObject->SetName(objectData.name);

            outObjects.push_back(std::move(newObject));
        }
    }

    for (const auto& childData : objectData.children) {
        CreateObjectRecursive(childData, device, modelCommon, baseDirectoryPath, outObjects);
    }
}

#ifdef USE_IMGUI
void LevelDataLoader::DisplayImGui(ID3D12Device* device, ModelCommon* modelCommon, std::vector<std::unique_ptr<Object3D>>& outObjects) {
    if (ImGui::TreeNode("Blender 3D Level Loader Sync")) {
        static std::vector<std::string> levelFiles = {
            "../tools/TL.json",
            "../tools/TL.scene",
            "tools/TL.json",
            "tools/TL.scene",
            "TL.json",
            "TL.scene"
        };
        static int selectedIndex = 0;
        static char inputFilenameBuf[512] = "../tools/TL.json";

        std::string comboPreview = (selectedIndex >= 0 && selectedIndex < (int)levelFiles.size()) ? levelFiles[selectedIndex] : "レベルファイルを選択...";
        if (ImGui::BeginCombo("レベルファイルを選択", comboPreview.c_str())) {
            for (int i = 0; i < (int)levelFiles.size(); ++i) {
                bool isSelected = (selectedIndex == i);
                if (ImGui::Selectable(levelFiles[i].c_str(), isSelected)) {
                    selectedIndex = i;
                    strcpy_s(inputFilenameBuf, sizeof(inputFilenameBuf), levelFiles[i].c_str());

                    if (LoadFile(levelFiles[i])) {
                        outObjects = CreateObjects(device, modelCommon);
                    }
                }
                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        if (ImGui::InputText("ファイル名", inputFilenameBuf, sizeof(inputFilenameBuf), ImGuiInputTextFlags_EnterReturnsTrue)) {
            if (LoadFile(inputFilenameBuf)) {
                outObjects = CreateObjects(device, modelCommon);
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("読み込み (Load Sync)")) {
            if (LoadFile(inputFilenameBuf)) {
                outObjects = CreateObjects(device, modelCommon);
            }
        }

        ImGui::Separator();
        ImGui::Text("Current Loaded File: %s", loadedFilePath_.c_str());
        ImGui::Text("Status: %s", isLoaded_ ? "Loaded Successfully" : "Not Loaded");

        if (isLoaded_) {
            // 有効・無効オブジェクト数をカウント
            int totalCount = 0;
            int disabledCount = 0;
            auto countStats = [](auto& self, const ObjectData& node, int& total, int& disabled) -> void {
                total++;
                if (node.disabled) disabled++;
                for (const auto& child : node.children) {
                    self(self, child, total, disabled);
                }
            };
            for (const auto& obj : levelData_.objects) {
                countStats(countStats, obj, totalCount, disabledCount);
            }

            ImGui::Text("Total Objects in JSON: %d", totalCount);
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Placed (Active) Objects: %d", (int)outObjects.size());
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Player Spawns Count: %d", (int)levelData_.players.size());
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "Enemy Spawns Count: %d", (int)levelData_.enemies.size());
            if (disabledCount > 0) {
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Disabled (Skipped) Objects: %d", disabledCount);
            } else {
                ImGui::Text("Disabled (Skipped) Objects: 0");
            }

            if (ImGui::TreeNode("Level Hierarchy (Blender JSON)")) {
                ImGui::Text("Scene Name: %s", levelData_.name.c_str());

                auto renderNodeImGui = [](auto& self, const ObjectData& node) -> void {
                    std::string label = node.name + " [" + node.type + "]";
                    if (node.disabled) {
                        label += " [Disabled - Skipped]";
                    }
                    if (!node.fileName.empty()) {
                        label += " (Model: " + node.fileName + ")";
                    }

                    if (node.disabled) {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
                    }

                    if (node.children.empty()) {
                        if (node.disabled) {
                            ImGui::TextDisabled("  - %s", label.c_str());
                        } else {
                            ImGui::BulletText("%s", label.c_str());
                        }
                    } else {
                        if (ImGui::TreeNode(label.c_str())) {
                            for (const auto& child : node.children) {
                                self(self, child);
                            }
                            ImGui::TreePop();
                        }
                    }

                    if (node.disabled) {
                        ImGui::PopStyleColor();
                    }
                };

                for (const auto& obj : levelData_.objects) {
                    renderNodeImGui(renderNodeImGui, obj);
                }

                ImGui::TreePop();
            }

            if (ImGui::TreeNode("Active Placed Objects (In Engine)")) {
                ImGui::Text("Active Objects Count: %d", (int)outObjects.size());
                for (size_t i = 0; i < outObjects.size(); ++i) {
                    if (outObjects[i]) {
                        ImGui::BulletText("[%d] %s", (int)i, outObjects[i]->GetName().c_str());
                    }
                }
                ImGui::TreePop();
            }
        }

        ImGui::TreePop();
    }
}
#endif
