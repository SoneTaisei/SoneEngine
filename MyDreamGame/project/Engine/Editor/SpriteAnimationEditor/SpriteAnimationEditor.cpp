#include "SpriteAnimationEditor.h"
#include <filesystem>
#include <algorithm>
#include <fstream>
#include <cmath>
#include <nlohmann/json.hpp>
#include "Graphics/TextureManager.h"
#include "Graphics/CameraManager.h"
#include "Scenes/GameScene.h"
#include "Scene/SceneManager.h"
#include "Platform/WindowsApplication.h"
#include "Game2D/MapChip2D.h"
#include "Renderer/DirectXCommon/DirectXCommon.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

SpriteAnimationEditor::SpriteAnimationEditor() {
}

void SpriteAnimationEditor::Initialize() {
    RefreshFileList();
    RefreshStageList();
}

std::string SpriteAnimationEditor::GetDefaultJsonPath(const std::string& imagePath) const {
    std::filesystem::path p(imagePath);
    p.replace_extension(".json");
    return p.generic_string();
}

void SpriteAnimationEditor::ApplyDefaultFrameSize() {
    float frameW = (columns_ > 0 && textureBaseSize_.x > 0.0f) ? (textureBaseSize_.x / static_cast<float>(columns_)) : 64.0f;
    float frameH = (rows_ > 0 && textureBaseSize_.y > 0.0f) ? (textureBaseSize_.y / static_cast<float>(rows_)) : 64.0f;
    placementSize_ = { frameW, frameH };
}

void SpriteAnimationEditor::OnFileSelected(int index) {
    if (index < 0 || index >= static_cast<int>(fileList_.size())) {
        return;
    }

    selectedFileIndex_ = index;
    const std::string& filePath = fileList_[selectedFileIndex_];

    loadedTextureHandle_ = TextureManager::GetInstance()->Load(filePath);
    const D3D12_RESOURCE_DESC resDesc = TextureManager::GetInstance()->GetResourceDesc(loadedTextureHandle_);
    textureBaseSize_ = { static_cast<float>(resDesc.Width), static_cast<float>(resDesc.Height) };
    currentFrame_ = 0;
    animTimer_ = 0.0f;

    std::string defaultJson = GetDefaultJsonPath(filePath);
    strncpy_s(customJsonPath_, sizeof(customJsonPath_), defaultJson.c_str(), _TRUNCATE);

    if (std::filesystem::exists(defaultJson)) {
        LoadConfigFromJson(defaultJson);
    } else {
        columns_ = 4;
        rows_ = 4;
        totalFrames_ = 16;
        fps_ = 15.0f;
        isLoop_ = true;
        ApplyDefaultFrameSize();
    }
}

void SpriteAnimationEditor::RefreshFileList() {
    fileList_.clear();
    selectedFileIndex_ = -1;
    loadedTextureHandle_ = 0;
    textureBaseSize_ = { 0.0f, 0.0f };

    std::filesystem::path dirPath(targetDirectory_);
    if (!std::filesystem::exists(dirPath)) {
        std::filesystem::create_directories(dirPath);
    }

    if (std::filesystem::exists(dirPath) && std::filesystem::is_directory(dirPath)) {
        for (const auto& entry : std::filesystem::directory_iterator(dirPath)) {
            if (entry.is_regular_file()) {
                std::string ext = entry.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".dds" || ext == ".tga" || ext == ".gif") {
                    std::string p = entry.path().generic_string();
                    fileList_.push_back(p);
                }
            }
        }
    }

    std::sort(fileList_.begin(), fileList_.end());

    if (!fileList_.empty()) {
        OnFileSelected(0);
    }
}

void SpriteAnimationEditor::RefreshStageList() {
    stageList_ = StageGifManager::GetInstance()->ScanAvailableStages();
    std::string currentStage = StageGifManager::GetInstance()->GetCurrentStageName();
    int foundIdx = -1;
    for (int i = 0; i < static_cast<int>(stageList_.size()); ++i) {
        if (stageList_[i] == currentStage) {
            foundIdx = i;
            break;
        }
    }
    if (foundIdx >= 0) {
        selectedStageIndex_ = foundIdx;
    } else if (!stageList_.empty()) {
        selectedStageIndex_ = 0;
        StageGifManager::GetInstance()->LoadForStage(stageList_[0]);
    }
    selectedPlacedGifIndex_ = -1;
}

void SpriteAnimationEditor::SyncWithActiveScene() {
    auto* stageMgr = StageGifManager::GetInstance();
    std::string sName = "";
    IScene* currentScene = nullptr;
    if (auto* app = WindowsApplication::GetInstance()) {
        if (auto* scnMgr = app->GetSceneManager()) {
            currentScene = scnMgr->GetCurrentScene();
        }
    }
    if (currentScene) {
        if (auto* mapChip = currentScene->GetMapChip()) {
            sName = stageMgr->ResolveStageName(mapChip->GetCurrentFilePath());
        }
    }
    if (sName.empty() || sName == "temp_play_map") {
        sName = stageMgr->ResolveStageName(GameScene::s_TargetMapFilePath);
    }
    if (sName.empty() || sName == "temp_play_map") {
        sName = "tutorial";
    }

    if (stageMgr->GetCurrentStageName() != sName) {
        stageMgr->LoadForStage(sName);
    }

    RefreshStageList();
    for (int i = 0; i < static_cast<int>(stageList_.size()); ++i) {
        if (stageList_[i] == sName) {
            selectedStageIndex_ = i;
            break;
        }
    }
    selectedPlacedGifIndex_ = -1;
}

bool SpriteAnimationEditor::SaveConfigToJson(const std::string& jsonPath) {
    if (selectedFileIndex_ < 0 || selectedFileIndex_ >= static_cast<int>(fileList_.size())) {
        return false;
    }

    nlohmann::json j;
    j["texturePath"] = fileList_[selectedFileIndex_];
    j["columns"] = columns_;
    j["rows"] = rows_;
    j["totalFrames"] = totalFrames_;
    j["fps"] = fps_;
    j["isLoop"] = isLoop_;
    j["size"] = { placementSize_.x, placementSize_.y };
    j["position"] = { placementPos_.x, placementPos_.y };
    j["color"] = { placementColor_.x, placementColor_.y, placementColor_.z, placementColor_.w };

    std::filesystem::path p(jsonPath);
    if (p.has_parent_path()) {
        std::filesystem::create_directories(p.parent_path());
    }

    std::ofstream ofs(jsonPath);
    if (!ofs.is_open()) {
        statusMessage_ = "保存に失敗しました: " + jsonPath;
        statusTimer_ = 4.0f;
        return false;
    }

    ofs << j.dump(4);
    statusMessage_ = "設定を保存しました: " + jsonPath;
    statusTimer_ = 4.0f;
    return true;
}

bool SpriteAnimationEditor::LoadConfigFromJson(const std::string& jsonPath) {
    if (!std::filesystem::exists(jsonPath)) {
        statusMessage_ = "ファイルが見つかりません: " + jsonPath;
        statusTimer_ = 4.0f;
        return false;
    }

    std::ifstream ifs(jsonPath);
    if (!ifs.is_open()) {
        statusMessage_ = "読み込みに失敗しました: " + jsonPath;
        statusTimer_ = 4.0f;
        return false;
    }

    nlohmann::json j;
    try {
        ifs >> j;
    } catch (...) {
        statusMessage_ = "JSON解析エラー: " + jsonPath;
        statusTimer_ = 4.0f;
        return false;
    }

    columns_ = j.value("columns", columns_);
    rows_ = j.value("rows", rows_);
    totalFrames_ = j.value("totalFrames", totalFrames_);
    fps_ = j.value("fps", fps_);
    isLoop_ = j.value("isLoop", isLoop_);

    if (j.contains("size") && j["size"].is_array() && j["size"].size() >= 2) {
        placementSize_.x = j["size"][0].get<float>();
        placementSize_.y = j["size"][1].get<float>();
    } else {
        ApplyDefaultFrameSize();
    }

    if (j.contains("position") && j["position"].is_array() && j["position"].size() >= 2) {
        placementPos_.x = j["position"][0].get<float>();
        placementPos_.y = j["position"][1].get<float>();
    }

    if (j.contains("color") && j["color"].is_array() && j["color"].size() >= 4) {
        placementColor_.x = j["color"][0].get<float>();
        placementColor_.y = j["color"][1].get<float>();
        placementColor_.z = j["color"][2].get<float>();
        placementColor_.w = j["color"][3].get<float>();
    }

    currentFrame_ = 0;
    animTimer_ = 0.0f;

    statusMessage_ = "設定を読み込みました: " + jsonPath;
    statusTimer_ = 4.0f;
    return true;
}

void SpriteAnimationEditor::Update(float deltaTime) {
    // シーン連動が有効な場合、アクティブなシーンのステージに自動追従
    if (autoSyncWithScene_) {
        auto* stageMgr = StageGifManager::GetInstance();
        std::string currentSceneStage = "";
        IScene* currentScene = nullptr;
        if (auto* app = WindowsApplication::GetInstance()) {
            if (auto* scnMgr = app->GetSceneManager()) {
                currentScene = scnMgr->GetCurrentScene();
            }
        }
        if (currentScene) {
            if (auto* mapChip = currentScene->GetMapChip()) {
                currentSceneStage = stageMgr->ResolveStageName(mapChip->GetCurrentFilePath());
            }
        }
        if (currentSceneStage.empty() || currentSceneStage == "temp_play_map") {
            currentSceneStage = stageMgr->ResolveStageName(GameScene::s_TargetMapFilePath);
        }
        if (!currentSceneStage.empty() && currentSceneStage != "temp_play_map") {
            if (stageMgr->GetCurrentStageName() != currentSceneStage) {
                stageMgr->LoadForStage(currentSceneStage);
                RefreshStageList();
            }
        }
    }

    // 3Dワールド配置GIFの更新（エディタ停止中もアニメーション再生＆トランスフォーム反映）
    StageGifManager::GetInstance()->Update(deltaTime);

    if (statusTimer_ > 0.0f) {
        statusTimer_ -= deltaTime;
        if (statusTimer_ <= 0.0f) {
            statusMessage_ = "";
        }
    }

    if (!isPlaying_ || totalFrames_ <= 1) {
        return;
    }

    float frameDuration = (fps_ > 0.0f) ? (1.0f / fps_) : 0.1f;
    animTimer_ += deltaTime;

    while (animTimer_ >= frameDuration) {
        animTimer_ -= frameDuration;
        int nextFrame = currentFrame_ + 1;
        if (nextFrame >= totalFrames_) {
            if (isLoop_) {
                nextFrame = 0;
            } else {
                nextFrame = totalFrames_ - 1;
                isPlaying_ = false;
                break;
            }
        }
        currentFrame_ = nextFrame;
    }
}

#ifdef USE_IMGUI
void SpriteAnimationEditor::DrawUI(bool* pOpen, const ImVec2& gameViewPos, const ImVec2& gameViewSize) {
    if (!pOpen || !*pOpen) return;

    if (ImGui::Begin("スプライト・GIFアニメーションエディター", pOpen)) {
        // モード切り替えタブ
        ImGui::SeparatorText("エディターモード切替");
        if (ImGui::RadioButton("3Dワールド配置 (板ポリ)", currentMode_ == EditorMode::World3D)) {
            currentMode_ = EditorMode::World3D;
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("2Dスプライト (UI)", currentMode_ == EditorMode::Sprite2D)) {
            currentMode_ = EditorMode::Sprite2D;
        }

        ImGui::Spacing();
        ImGui::Separator();

        // 共通：ファイル選択とアニメーションプレビュー
        ImGui::Text("【GIFフォルダ】: %s", targetDirectory_.c_str());
        ImGui::SameLine();
        if (ImGui::Button("一覧を更新 (Refresh)")) {
            RefreshFileList();
        }

        if (fileList_.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "フォルダ内に画像ファイルがありません。");
            ImGui::TextWrapped("'%s' フォルダにスプライトシート（PNGやGIFなど）を配置してください。", targetDirectory_.c_str());
            ImGui::End();
            return;
        }

        std::string currentFileName = "";
        if (selectedFileIndex_ >= 0 && selectedFileIndex_ < static_cast<int>(fileList_.size())) {
            currentFileName = std::filesystem::path(fileList_[selectedFileIndex_]).filename().string();
        }

        if (ImGui::BeginCombo("選択中のGIF画像", currentFileName.c_str())) {
            for (int i = 0; i < static_cast<int>(fileList_.size()); ++i) {
                std::string fname = std::filesystem::path(fileList_[i]).filename().string();
                bool isSelected = (selectedFileIndex_ == i);
                if (ImGui::Selectable(fname.c_str(), isSelected)) {
                    if (selectedFileIndex_ != i) {
                        OnFileSelected(i);
                    }
                }
                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        ImGui::Text("画像解像度: %.0f x %.0f px", textureBaseSize_.x, textureBaseSize_.y);

        if (ImGui::CollapsingHeader("アニメーション分割・再生プレビュー設定", ImGuiTreeNodeFlags_DefaultOpen)) {
            bool gridChanged = false;
            int prevMaxFrames = columns_ * rows_;
            if (ImGui::SliderInt("横分割数 (Columns)", &columns_, 1, 32)) {
                gridChanged = true;
            }
            if (ImGui::SliderInt("縦分割数 (Rows)", &rows_, 1, 32)) {
                gridChanged = true;
            }

            int maxPossibleFrames = columns_ * rows_;
            if (gridChanged) {
                // 分割数を変更した際、1コマ以下だった場合や以前の全コマ数だった場合は自動で全コマ数に更新
                if (totalFrames_ <= 1 || totalFrames_ == prevMaxFrames || totalFrames_ > maxPossibleFrames) {
                    totalFrames_ = maxPossibleFrames;
                }
                currentFrame_ = 0;
            }
            if (totalFrames_ <= 0) totalFrames_ = 1;
            if (totalFrames_ > maxPossibleFrames) totalFrames_ = maxPossibleFrames;

            ImGui::SliderInt("総コマ数 (TotalFrames)", &totalFrames_, 1, maxPossibleFrames);

            float frameW = (columns_ > 0 && textureBaseSize_.x > 0.0f) ? (textureBaseSize_.x / static_cast<float>(columns_)) : 0.0f;
            float frameH = (rows_ > 0 && textureBaseSize_.y > 0.0f) ? (textureBaseSize_.y / static_cast<float>(rows_)) : 0.0f;
            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.4f, 1.0f), "1コマのサイズ: %.1f x %.1f px (計 %d コマ)", frameW, frameH, totalFrames_);

            ImGui::SliderFloat("再生速度 (FPS)", &fps_, 1.0f, 60.0f, "%.1f FPS");
            ImGui::Checkbox("ループ再生 (Loop)", &isLoop_);

            if (isPlaying_) {
                if (ImGui::Button("[一時停止]")) { isPlaying_ = false; }
            } else {
                if (ImGui::Button("[再生]")) { isPlaying_ = true; }
            }
            ImGui::SameLine();
            if (ImGui::Button("[停止]")) {
                isPlaying_ = false;
                currentFrame_ = 0;
                animTimer_ = 0.0f;
            }

            if (ImGui::SliderInt("シークバー (Frame)", &currentFrame_, 0, totalFrames_ - 1)) {
                animTimer_ = 0.0f;
            }

            // UV切り抜きプレビュー
            if (loadedTextureHandle_ != 0 && textureBaseSize_.x > 0.0f && textureBaseSize_.y > 0.0f) {
                int col = currentFrame_ % columns_;
                int row = currentFrame_ / columns_;
                ImVec2 uv0 = ImVec2((col * frameW) / textureBaseSize_.x, (row * frameH) / textureBaseSize_.y);
                ImVec2 uv1 = ImVec2(((col + 1) * frameW) / textureBaseSize_.x, ((row + 1) * frameH) / textureBaseSize_.y);

                ImVec2 previewSize = ImVec2(frameW * previewScale_, frameH * previewScale_);
                D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = TextureManager::GetInstance()->GetGpuHandle(loadedTextureHandle_);

                ImGui::SliderFloat("プレビュースケール", &previewScale_, 0.25f, 4.0f, "%.2fx");
                ImGui::Text("フレーム: %d / %d", currentFrame_ + 1, totalFrames_);
                ImGui::Image((ImTextureID)gpuHandle.ptr, previewSize, uv0, uv1, ImVec4(1.0f, 1.0f, 1.0f, 1.0f), ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
            }
        }

        ImGui::Spacing();
        ImGui::Separator();

        // モード別描画
        if (currentMode_ == EditorMode::World3D) {
            DrawWorldPlacementTab(gameViewPos, gameViewSize);
        } else {
            Draw2DSpriteTab(gameViewPos, gameViewSize);
        }
    }
    ImGui::End();
}

void SpriteAnimationEditor::DrawWorldPlacementTab(const ImVec2& gameViewPos, const ImVec2& gameViewSize) {
    ImGui::SeparatorText("ステージ別ワールド板ポリ配置");

    auto* stageMgr = StageGifManager::GetInstance();

    // 1. ステージ選択
    if (stageList_.empty()) {
        RefreshStageList();
    }

    std::string currentStage = stageMgr->GetCurrentStageName();
    if (ImGui::BeginCombo("対象ステージ", currentStage.c_str())) {
        for (int i = 0; i < static_cast<int>(stageList_.size()); ++i) {
            bool isSelected = (stageList_[i] == currentStage);
            if (ImGui::Selectable(stageList_[i].c_str(), isSelected)) {
                selectedStageIndex_ = i;
                autoSyncWithScene_ = false; // 手動選択時は自動同期を解除
                stageMgr->LoadForStage(stageList_[i]);
                selectedPlacedGifIndex_ = -1;
            }
            if (isSelected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (ImGui::Button("[現在のシーンに同期]")) {
        SyncWithActiveScene();
    }

    ImGui::Checkbox("シーンのマップに自動同期", &autoSyncWithScene_);
    ImGui::SameLine();
    bool showAll = stageMgr->GetShowAllStages();
    if (ImGui::Checkbox("ステージに関係なく全GIFを表示", &showAll)) {
        stageMgr->SetShowAllStages(showAll);
        stageMgr->LoadForStage(currentStage);
    }

    // ステージ保存・読込・インポートボタン
    if (ImGui::Button("[このステージのGIF配置を保存 (Save)]")) {
        stageMgr->SaveForStage(currentStage);
        statusMessage_ = "ステージのGIF配置を保存しました: " + currentStage + "_gifs.json";
        statusTimer_ = 4.0f;
    }
    ImGui::SameLine();
    if (ImGui::Button("[再読み込み (Reload)]")) {
        stageMgr->LoadForStage(currentStage);
        selectedPlacedGifIndex_ = -1;
        statusMessage_ = "再読み込みしました: " + currentStage;
        statusTimer_ = 3.0f;
    }
    ImGui::SameLine();
    if (ImGui::Button("[他ステージからインポート]")) {
        ImGui::OpenPopup("ImportStagePopup");
    }

    if (ImGui::BeginPopup("ImportStagePopup")) {
        ImGui::Text("インポート元ステージを選択:");
        ImGui::Separator();
        for (const auto& other : stageList_) {
            if (other == currentStage) continue;
            if (ImGui::Selectable(other.c_str())) {
                stageMgr->ImportFromStage(other);
                statusMessage_ = other + " からGIFをインポートしました";
                statusTimer_ = 4.0f;
            }
        }
        ImGui::EndPopup();
    }

    if (!statusMessage_.empty()) {
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.3f, 1.0f), "%s", statusMessage_.c_str());
    }

    ImGui::Spacing();
    ImGui::SeparatorText("ステージ内の配置GIFリスト");

    auto& objects = stageMgr->GetObjects();

    // 追加・削除・複製ボタン
    if (ImGui::Button("[+ 現在のGIFをステージに配置]")) {
        if (selectedFileIndex_ >= 0 && selectedFileIndex_ < static_cast<int>(fileList_.size())) {
            WorldGifConfig cfg;
            std::string baseStem = std::filesystem::path(fileList_[selectedFileIndex_]).stem().string();
            cfg.name = baseStem + "_" + std::to_string(objects.size() + 1);
            cfg.texturePath = fileList_[selectedFileIndex_];
            cfg.columns = columns_;
            cfg.rows = rows_;
            cfg.totalFrames = totalFrames_;
            cfg.fps = fps_;
            cfg.isLoop = isLoop_;

            // プレイヤー位置またはカメラ注視点付近に配置
            Vector3 camPos = CameraManager::GetInstance()->GetCameraPos();
            cfg.translation = { camPos.x, camPos.y, 0.0f };

            // アスペクト比を反映したスケール (起立時にXが横幅、Zが縦幅となるためZにも高さを設定)
            float frameW = (columns_ > 0 && textureBaseSize_.x > 0.0f) ? (textureBaseSize_.x / static_cast<float>(columns_)) : 1.0f;
            float frameH = (rows_ > 0 && textureBaseSize_.y > 0.0f) ? (textureBaseSize_.y / static_cast<float>(rows_)) : 1.0f;
            float aspect = (frameH > 0.0f) ? (frameW / frameH) : 1.0f;
            cfg.scale = { 2.0f * aspect, 2.0f, 2.0f };
            cfg.rotation = { -1.5707963f, 0.0f, 0.0f }; // 2.5D垂直配置
            cfg.isDoubleSided = true;

            WorldGifObject* newObj = stageMgr->AddGifObject(cfg);
            if (newObj) {
                selectedPlacedGifIndex_ = static_cast<int>(objects.size()) - 1;
                statusMessage_ = "GIFをステージに追加しました: " + cfg.name;
                statusTimer_ = 3.0f;
            }
        }
    }

    ImGui::SameLine();
    bool hasSelection = (selectedPlacedGifIndex_ >= 0 && selectedPlacedGifIndex_ < static_cast<int>(objects.size()));
    if (!hasSelection) ImGui::BeginDisabled();
    if (ImGui::Button("[- 削除]")) {
        if (hasSelection) {
            stageMgr->RemoveGifObject(objects[selectedPlacedGifIndex_].get());
            selectedPlacedGifIndex_ = -1;
            statusMessage_ = "GIFをステージから削除しました";
            statusTimer_ = 3.0f;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("[複製]")) {
        if (hasSelection) {
            WorldGifObject* dup = stageMgr->DuplicateGifObject(objects[selectedPlacedGifIndex_].get());
            if (dup) {
                selectedPlacedGifIndex_ = static_cast<int>(objects.size()) - 1;
            }
        }
    }
    if (!hasSelection) ImGui::EndDisabled();

    // 削除・追加後の安全なインデックス再検証
    if (selectedPlacedGifIndex_ < 0 || selectedPlacedGifIndex_ >= static_cast<int>(objects.size())) {
        selectedPlacedGifIndex_ = -1;
    }
    bool isObjectSelected = (selectedPlacedGifIndex_ >= 0 && selectedPlacedGifIndex_ < static_cast<int>(objects.size()) && objects[selectedPlacedGifIndex_]);
    if (!isObjectSelected) {
        isDraggingGizmo_ = false;
        gizmoActiveAxis_ = -1;
    }

    // 配置オブジェクト一覧リスト
    if (ImGui::BeginListBox("##PlacedGifsList", ImVec2(-1.0f, 120.0f))) {
        for (int i = 0; i < static_cast<int>(objects.size()); ++i) {
            auto* obj = objects[i].get();
            if (!obj) continue;
            const auto& cfg = obj->GetConfig();
            char label[256];
            std::string allTag = cfg.showInAllStages ? " [全ステージ共通]" : "";
            snprintf(label, sizeof(label), "#%d: %s%s (%.1f, %.1f, %.1f)", i + 1, cfg.name.c_str(), allTag.c_str(), cfg.translation.x, cfg.translation.y, cfg.translation.z);

            bool isSelected = (selectedPlacedGifIndex_ == i);
            if (ImGui::Selectable(label, isSelected)) {
                selectedPlacedGifIndex_ = i;
            }
            if (isSelected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndListBox();
    }

    // 選択オブジェクトのインスペクター
    if (isObjectSelected) {
        auto* selObj = objects[selectedPlacedGifIndex_].get();
        auto& cfg = selObj->GetConfig();

        ImGui::Spacing();
        ImGui::SeparatorText("選択中GIFのプロパティ (Inspector)");

        char nameBuf[128];
        strncpy_s(nameBuf, sizeof(nameBuf), cfg.name.c_str(), _TRUNCATE);
        if (ImGui::InputText("名前 (Name)", nameBuf, sizeof(nameBuf))) {
            cfg.name = nameBuf;
        }

        bool showInAll = cfg.showInAllStages;
        if (ImGui::Checkbox("全ステージ共通で表示 (Show in all stages)", &showInAll)) {
            cfg.showInAllStages = showInAll;
            selObj->SetConfig(cfg, DirectXCommon::GetInstance()->GetDevice());
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("チェックを入れると、どのステージを読み込んでもこのGIFが共通して表示されます");
        }

        ImGui::Spacing();
        ImGui::Text("【トランスフォーム設定 (矢印ギズモ連動)】");
        ImGui::Checkbox("ゲーム画面に矢印ギズモを表示", &showTransformGizmo_);
        ImGui::SameLine();
        ImGui::Checkbox("スナップ", &isSnapEnabled_);
        if (isSnapEnabled_) {
            ImGui::SameLine();
            ImGui::SetNextItemWidth(80.0f);
            ImGui::DragFloat("刻み", &snapStep_, 0.1f, 0.1f, 5.0f, "%.1f");
        }

        Vector3 pos = selObj->GetTranslation();
        if (ImGui::DragFloat3("位置 (Translation X, Y, Z)", &pos.x, 0.05f)) {
            selObj->SetTranslation(pos);
        }

        Vector3 rotDeg = {
            cfg.rotation.x * 57.2957795f,
            cfg.rotation.y * 57.2957795f,
            cfg.rotation.z * 57.2957795f
        };
        if (ImGui::DragFloat3("回転 (Rotation Deg X, Y, Z)", &rotDeg.x, 1.0f)) {
            Vector3 newRot = {
                rotDeg.x * 0.0174532925f,
                rotDeg.y * 0.0174532925f,
                rotDeg.z * 0.0174532925f
            };
            selObj->SetRotation(newRot);
        }

        // サイズ/スケール調整
        // PrimitivePlaneはXZ平面のため、横幅=scale.x, 縦幅=scale.z (起立時Y軸)
        float currentH = (cfg.scale.z != 1.0f && cfg.scale.z != cfg.scale.y) ? cfg.scale.z : cfg.scale.y;
        Vector2 size2D = { cfg.scale.x, currentH };
        if (ImGui::DragFloat2("サイズ (横幅 X, 縦幅 Y)", &size2D.x, 0.05f, 0.01f, 100.0f)) {
            cfg.scale.x = size2D.x;
            cfg.scale.y = size2D.y;
            cfg.scale.z = size2D.y;
            selObj->SetScale(cfg.scale);
        }
        if (ImGui::TreeNode("詳細スケール (3D軸 X, Y, Z)")) {
            Vector3 sc = selObj->GetScale();
            if (ImGui::DragFloat3("スケール (Scale X, Y, Z)", &sc.x, 0.05f, 0.01f, 100.0f)) {
                selObj->SetScale(sc);
            }
            ImGui::TreePop();
        }

        bool bb = cfg.isBillboard;
        if (ImGui::Checkbox("常にカメラを向く (Billboard)", &bb)) {
            selObj->SetBillboard(bb);
        }
        ImGui::SameLine();
        bool ds = cfg.isDoubleSided;
        if (ImGui::Checkbox("両面描画 (Double-Sided)", &ds)) {
            selObj->SetDoubleSided(ds);
        }

        Vector4 col = cfg.color;
        if (ImGui::ColorEdit4("カラー (Color RGBA)", &col.x)) {
            selObj->SetColor(col);
        }

        ImGui::Spacing();
        ImGui::SeparatorText("アニメーション設定 (リアルタイム反映)");
        ImGui::Text("画像パス: %s", cfg.texturePath.c_str());

        bool animCfgChanged = false;
        int placedPrevMax = cfg.columns * cfg.rows;
        if (ImGui::SliderInt("配置GIF 横分割数 (Columns / 横コマ)", &cfg.columns, 1, 32)) {
            if (cfg.totalFrames <= 1 || cfg.totalFrames == placedPrevMax || cfg.totalFrames > cfg.columns * cfg.rows) {
                cfg.totalFrames = cfg.columns * cfg.rows;
            }
            animCfgChanged = true;
        }
        if (ImGui::SliderInt("配置GIF 縦分割数 (Rows / 縦コマ)", &cfg.rows, 1, 32)) {
            if (cfg.totalFrames <= 1 || cfg.totalFrames == placedPrevMax || cfg.totalFrames > cfg.columns * cfg.rows) {
                cfg.totalFrames = cfg.columns * cfg.rows;
            }
            animCfgChanged = true;
        }
        int placedMaxFrames = cfg.columns * cfg.rows;
        if (cfg.totalFrames <= 0) cfg.totalFrames = 1;
        if (cfg.totalFrames > placedMaxFrames) cfg.totalFrames = placedMaxFrames;
        if (ImGui::SliderInt("配置GIF 総コマ数 (TotalFrames)", &cfg.totalFrames, 1, placedMaxFrames)) {
            animCfgChanged = true;
        }
        if (ImGui::SliderFloat("配置GIF 再生速度 (FPS)", &cfg.fps, 1.0f, 60.0f, "%.1f FPS")) {
            animCfgChanged = true;
        }
        if (ImGui::Checkbox("配置GIF ループ再生 (Loop)", &cfg.isLoop)) {
            animCfgChanged = true;
        }

        if (animCfgChanged) {
            selObj->SetConfig(cfg, DirectXCommon::GetInstance()->GetDevice());
        }

        ImGui::Spacing();
        if (ImGui::Button("[エディター上部のプレビュー設定をこのGIFに反映]")) {
            if (selectedFileIndex_ >= 0) {
                cfg.texturePath = fileList_[selectedFileIndex_];
                cfg.columns = columns_;
                cfg.rows = rows_;
                cfg.totalFrames = totalFrames_;
                cfg.fps = fps_;
                cfg.isLoop = isLoop_;
                selObj->SetConfig(cfg, DirectXCommon::GetInstance()->GetDevice());
                statusMessage_ = "アニメーション設定を反映しました";
                statusTimer_ = 3.0f;
            }
        }
    }

    // 矢印ギズモ描画
    if (showTransformGizmo_ && isObjectSelected) {
        DrawTransformGizmo(gameViewPos, gameViewSize, objects[selectedPlacedGifIndex_].get());
    }
}

void SpriteAnimationEditor::DrawTransformGizmo(const ImVec2& vpPos, const ImVec2& vpSize, WorldGifObject* selectedObj) {
    if (!selectedObj || vpSize.x <= 0.0f || vpSize.y <= 0.0f) return;

    Matrix4x4 viewMat = CameraManager::GetInstance()->GetViewMatrix();
    Matrix4x4 projMat = CameraManager::GetInstance()->GetProjectionMatrix();
    Matrix4x4 vpMat = TransformFunctions::Multiply(viewMat, projMat);

    Vector3 origin = selectedObj->GetTranslation();

    auto project = [&](const Vector3& p, ImVec2& outP, float& outW) -> bool {
        Vector4 c;
        c.x = p.x * vpMat.m[0][0] + p.y * vpMat.m[1][0] + p.z * vpMat.m[2][0] + vpMat.m[3][0];
        c.y = p.x * vpMat.m[0][1] + p.y * vpMat.m[1][1] + p.z * vpMat.m[2][1] + vpMat.m[3][1];
        c.z = p.x * vpMat.m[0][2] + p.y * vpMat.m[1][2] + p.z * vpMat.m[2][2] + vpMat.m[3][2];
        c.w = p.x * vpMat.m[0][3] + p.y * vpMat.m[1][3] + p.z * vpMat.m[2][3] + vpMat.m[3][3];
        outW = c.w;
        if (c.w <= 0.05f) return false;
        outP.x = vpPos.x + (c.x / c.w + 1.0f) * 0.5f * vpSize.x;
        outP.y = vpPos.y + (1.0f - c.y / c.w) * 0.5f * vpSize.y;
        return true;
    };

    ImVec2 screenOrigin;
    float clipOriginW;
    if (!project(origin, screenOrigin, clipOriginW)) {
        return;
    }

    auto distToSegment = [](ImVec2 p, ImVec2 a, ImVec2 b) -> float {
        float l2 = (b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y);
        if (l2 < 1e-4f) return std::sqrt((p.x - a.x) * (p.x - a.x) + (p.y - a.y) * (p.y - a.y));
        float t = std::clamp(((p.x - a.x) * (b.x - a.x) + (p.y - a.y) * (b.y - a.y)) / l2, 0.0f, 1.0f);
        ImVec2 proj = ImVec2(a.x + t * (b.x - a.x), a.y + t * (b.y - a.y));
        return std::sqrt((p.x - proj.x) * (p.x - proj.x) + (p.y - proj.y) * (p.y - proj.y));
    };

    Vector3 axes[3] = {
        { 1.0f, 0.0f, 0.0f }, // X: 赤
        { 0.0f, 1.0f, 0.0f }, // Y: 緑
        { 0.0f, 0.0f, 1.0f }  // Z: 青
    };

    const ImU32 axisColors[3] = {
        IM_COL32(235, 60, 60, 240),
        IM_COL32(60, 220, 60, 240),
        IM_COL32(60, 140, 255, 240)
    };
    const ImU32 axisHoverColors[3] = {
        IM_COL32(255, 140, 140, 255),
        IM_COL32(140, 255, 140, 255),
        IM_COL32(140, 200, 255, 255)
    };
    const char* axisNames[3] = { "X", "Y", "Z" };

    const float kGizmoLength = 1.8f;
    ImVec2 screenTips[3];
    bool tipsValid[3] = { false, false, false };

    ImGuiIO& io = ImGui::GetIO();
    ImVec2 mousePos = io.MousePos;
    int hoveredAxis = -1;
    float minD = 12.0f;

    for (int i = 0; i < 3; ++i) {
        Vector3 tipPos = origin + axes[i] * kGizmoLength;
        float tipW;
        if (project(tipPos, screenTips[i], tipW)) {
            tipsValid[i] = true;
            float d = distToSegment(mousePos, screenOrigin, screenTips[i]);
            if (d < minD) {
                minD = d;
                hoveredAxis = i;
            }
        }
    }

    if (!isDraggingGizmo_ && hoveredAxis >= 0 && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        isDraggingGizmo_ = true;
        gizmoActiveAxis_ = hoveredAxis;
        gizmoDragStartPos_ = selectedObj->GetTranslation();
        gizmoDragStartMouse_ = { mousePos.x, mousePos.y };

        // 軸ベクトルの画面投影方向と長さ（ピクセル数）をドラッグ開始時に固定保持
        Vector2 dir = { screenTips[hoveredAxis].x - screenOrigin.x, screenTips[hoveredAxis].y - screenOrigin.y };
        float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
        if (len > 1.0f) {
            gizmoDragAxisLength2D_ = len;
            gizmoDragAxisDir2D_ = { dir.x / len, dir.y / len };
        } else {
            gizmoDragAxisLength2D_ = 1.0f;
            gizmoDragAxisDir2D_ = { 1.0f, 0.0f };
        }
    }

    if (isDraggingGizmo_ && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        isDraggingGizmo_ = false;
        gizmoActiveAxis_ = -1;
    }

    // ドラッグ移動処理: マウス移動ピクセル数を幾何学的に1対1でワールド変位へ換算
    if (isDraggingGizmo_ && gizmoActiveAxis_ >= 0 && gizmoActiveAxis_ < 3) {
        int a = gizmoActiveAxis_;
        // ドラッグ開始時からのマウス移動量（画面ピクセル単位）
        Vector2 mouseDelta = { mousePos.x - gizmoDragStartMouse_.x, mousePos.y - gizmoDragStartMouse_.y };

        // ギズモ軸方向への射影（ピクセル単位）
        float projPixel = mouseDelta.x * gizmoDragAxisDir2D_.x + mouseDelta.y * gizmoDragAxisDir2D_.y;

        // 画面上での gizmoDragAxisLength2D_ ピクセル = 3D空間上の kGizmoLength
        // したがって 1ピクセル = kGizmoLength / gizmoDragAxisLength2D_
        float worldPerPixel = kGizmoLength / (std::max)(gizmoDragAxisLength2D_, 1.0f);
        float totalWorldDelta = projPixel * worldPerPixel;

        if (isSnapEnabled_) {
            float s = (snapStep_ > 0.01f) ? snapStep_ : 0.5f;
            totalWorldDelta = std::round(totalWorldDelta / s) * s;
        }

        Vector3 newPos = gizmoDragStartPos_;
        newPos.x += axes[a].x * totalWorldDelta;
        newPos.y += axes[a].y * totalWorldDelta;
        newPos.z += axes[a].z * totalWorldDelta;

        selectedObj->SetTranslation(newPos);
    }

    // ギズモ描画
    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    for (int i = 0; i < 3; ++i) {
        if (!tipsValid[i]) continue;
        bool isAct = (gizmoActiveAxis_ == i) || (!isDraggingGizmo_ && hoveredAxis == i);
        ImU32 col = isAct ? axisHoverColors[i] : axisColors[i];
        float thick = isAct ? 3.0f : 2.0f;

        ImVec2 dir2D = ImVec2(screenTips[i].x - screenOrigin.x, screenTips[i].y - screenOrigin.y);
        float len2D = std::sqrt(dir2D.x * dir2D.x + dir2D.y * dir2D.y);
        if (len2D > 1.0f) {
            dir2D.x /= len2D;
            dir2D.y /= len2D;
        } else {
            dir2D = ImVec2(1.0f, 0.0f);
        }
        ImVec2 perp2D = ImVec2(-dir2D.y, dir2D.x);

        float arrowLen = isAct ? 12.0f : 9.0f;
        float arrowWidth = isAct ? 4.5f : 3.5f;

        ImVec2 apex = screenTips[i];
        ImVec2 baseCenter = ImVec2(apex.x - dir2D.x * arrowLen, apex.y - dir2D.y * arrowLen);
        ImVec2 baseL = ImVec2(baseCenter.x + perp2D.x * arrowWidth, baseCenter.y + perp2D.y * arrowWidth);
        ImVec2 baseR = ImVec2(baseCenter.x - perp2D.x * arrowWidth, baseCenter.y - perp2D.y * arrowWidth);

        drawList->AddLine(screenOrigin, baseCenter, col, thick);
        drawList->AddTriangleFilled(apex, baseL, baseR, col);
        drawList->AddTriangle(apex, baseL, baseR, IM_COL32(20, 20, 20, 240), 1.0f);
        drawList->AddText(ImVec2(apex.x + 4.0f, apex.y - 8.0f), col, axisNames[i]);
    }

    // 中心の目印
    drawList->AddCircleFilled(screenOrigin, 4.0f, IM_COL32(255, 255, 255, 220));
    drawList->AddCircle(screenOrigin, 4.0f, IM_COL32(0, 0, 0, 255), 0, 1.5f);
}

void SpriteAnimationEditor::Draw2DSpriteTab(const ImVec2& gameViewPos, const ImVec2& gameViewSize) {
    ImGui::SeparatorText("2Dスプライト (UI) 配置設定");

    ImGui::Checkbox("ゲーム画面にテスト表示 (Overlay in Game)", &showInGameOverlay_);
    ImGui::DragFloat2("配置座標 (X, Y)", &placementPos_.x, 1.0f, 0.0f, 1920.0f, "%.1f");
    ImGui::DragFloat2("表示サイズ (Width, Height)", &placementSize_.x, 1.0f, 1.0f, 1920.0f, "%.1f");
    ImGui::SameLine();
    if (ImGui::Button("[コマ原寸に合わせる]")) {
        ApplyDefaultFrameSize();
    }
    ImGui::ColorEdit4("表示カラー (RGBA)", &placementColor_.x);

    ImGui::Spacing();
    ImGui::SeparatorText("設定の保存と読込 (JSON)");

    ImGui::InputText("JSONファイルパス", customJsonPath_, sizeof(customJsonPath_));
    if (ImGui::Button("[設定を保存 (Save JSON)]")) {
        SaveConfigToJson(customJsonPath_);
    }
    ImGui::SameLine();
    if (ImGui::Button("[設定を読込 (Load JSON)]")) {
        LoadConfigFromJson(customJsonPath_);
    }
    ImGui::SameLine();
    if (ImGui::Button("[初期値にリセット]")) {
        if (selectedFileIndex_ >= 0) {
            OnFileSelected(selectedFileIndex_);
        }
    }

    if (!statusMessage_.empty()) {
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.3f, 1.0f), "%s", statusMessage_.c_str());
    }

    ImGui::Spacing();
    ImGui::SeparatorText("ゲームへの落とし込み用 C++ コード");

    if (ImGui::RadioButton("JSON読込方式 (推奨)", &codeTabSelection_, 0)) {}
    ImGui::SameLine();
    if (ImGui::RadioButton("C++手動指定方式", &codeTabSelection_, 1)) {}

    if (selectedFileIndex_ >= 0 && selectedFileIndex_ < static_cast<int>(fileList_.size())) {
        char codeBuf[1024];

        if (codeTabSelection_ == 0) {
            snprintf(
                codeBuf,
                sizeof(codeBuf),
                "// ==========================================================\n"
                "// [1] ヘッダー (例: GameScene.h)\n"
                "// ==========================================================\n"
                "#include \"Resource/Sprite/Sprite.h\"\n\n"
                "std::unique_ptr<Sprite> gifSprite_;\n\n"
                "// ==========================================================\n"
                "// [2] 初期化 (例: GameScene::Initialize)\n"
                "// ==========================================================\n"
                "gifSprite_ = std::make_unique<Sprite>();\n"
                "gifSprite_->InitializeFromConfig(spriteCommon_, \"%s\");\n\n"
                "// ==========================================================\n"
                "// [3] 更新 (例: GameScene::Update)\n"
                "// ==========================================================\n"
                "if (gifSprite_) {\n"
                "    gifSprite_->Update();\n"
                "}\n\n"
                "// ==========================================================\n"
                "// [4] 描画 (例: GameScene::Draw2D)\n"
                "// ==========================================================\n"
                "if (spriteCommon_ && gifSprite_) {\n"
                "    spriteCommon_->PreDraw();\n"
                "    gifSprite_->Draw();\n"
                "}\n",
                customJsonPath_
            );
        } else {
            snprintf(
                codeBuf,
                sizeof(codeBuf),
                "// ==========================================================\n"
                "// [1] ヘッダー (例: GameScene.h)\n"
                "// ==========================================================\n"
                "#include \"Resource/Sprite/Sprite.h\"\n\n"
                "std::unique_ptr<Sprite> gifSprite_;\n\n"
                "// ==========================================================\n"
                "// [2] 初期化 (例: GameScene::Initialize)\n"
                "// ==========================================================\n"
                "uint32_t texHandle = TextureManager::GetInstance()->Load(\"%s\");\n"
                "gifSprite_ = std::make_unique<Sprite>();\n"
                "gifSprite_->Initialize(spriteCommon_, texHandle);\n"
                "gifSprite_->SetAnimationGrid(%d, %d, %.1ff, %s, %d);\n"
                "gifSprite_->SetPosition({ %.1ff, %.1ff });\n"
                "gifSprite_->SetSize({ %.1ff, %.1ff });\n"
                "gifSprite_->SetColor({ %.2ff, %.2ff, %.2ff, %.2ff });\n"
                "gifSprite_->PlayAnimation();\n\n"
                "// ==========================================================\n"
                "// [3] 更新 (例: GameScene::Update)\n"
                "// ==========================================================\n"
                "if (gifSprite_) {\n"
                "    gifSprite_->Update();\n"
                "}\n\n"
                "// ==========================================================\n"
                "// [4] 描画 (例: GameScene::Draw2D)\n"
                "// ==========================================================\n"
                "if (spriteCommon_ && gifSprite_) {\n"
                "    spriteCommon_->PreDraw();\n"
                "    gifSprite_->Draw();\n"
                "}\n",
                fileList_[selectedFileIndex_].c_str(),
                columns_,
                rows_,
                fps_,
                isLoop_ ? "true" : "false",
                totalFrames_,
                placementPos_.x,
                placementPos_.y,
                placementSize_.x,
                placementSize_.y,
                placementColor_.x,
                placementColor_.y,
                placementColor_.z,
                placementColor_.w
            );
        }

        ImGui::InputTextMultiline("##GeneratedCode", codeBuf, sizeof(codeBuf), ImVec2(-1.0f, 150.0f), ImGuiInputTextFlags_ReadOnly);

        if (ImGui::Button("[C++コードをクリップボードにコピー]")) {
            ImGui::SetClipboardText(codeBuf);
            statusMessage_ = "コードをクリップボードにコピーしました！";
            statusTimer_ = 3.0f;
        }
    }

    // 2Dオーバーレイ描画
    if (showInGameOverlay_ && loadedTextureHandle_ != 0 && gameViewSize.x > 0.0f && gameViewSize.y > 0.0f) {
        float frameW = (columns_ > 0 && textureBaseSize_.x > 0.0f) ? (textureBaseSize_.x / static_cast<float>(columns_)) : 0.0f;
        float frameH = (rows_ > 0 && textureBaseSize_.y > 0.0f) ? (textureBaseSize_.y / static_cast<float>(rows_)) : 0.0f;

        if (frameW > 0.0f && frameH > 0.0f) {
            float scaleX = gameViewSize.x / 1280.0f;
            float scaleY = gameViewSize.y / 720.0f;

            ImVec2 screenP0 = ImVec2(gameViewPos.x + placementPos_.x * scaleX, gameViewPos.y + placementPos_.y * scaleY);
            ImVec2 screenP1 = ImVec2(screenP0.x + placementSize_.x * scaleX, screenP0.y + placementSize_.y * scaleY);

            int col = currentFrame_ % columns_;
            int row = currentFrame_ / columns_;

            ImVec2 uv0 = ImVec2((col * frameW) / textureBaseSize_.x, (row * frameH) / textureBaseSize_.y);
            ImVec2 uv1 = ImVec2(((col + 1) * frameW) / textureBaseSize_.x, ((row + 1) * frameH) / textureBaseSize_.y);

            ImDrawList* drawList = ImGui::GetForegroundDrawList();
            D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = TextureManager::GetInstance()->GetGpuHandle(loadedTextureHandle_);
            ImU32 tintCol = ImColor(placementColor_.x, placementColor_.y, placementColor_.z, placementColor_.w);

            drawList->AddImage((ImTextureID)gpuHandle.ptr, screenP0, screenP1, uv0, uv1, tintCol);
            drawList->AddRect(screenP0, screenP1, IM_COL32(255, 220, 0, 180), 0.0f, 0, 1.5f);
            drawList->AddText(ImVec2(screenP0.x + 2.0f, screenP0.y - 16.0f), IM_COL32(255, 220, 0, 220), "[2D GIF Overlay]");
        }
    }
}
#endif
