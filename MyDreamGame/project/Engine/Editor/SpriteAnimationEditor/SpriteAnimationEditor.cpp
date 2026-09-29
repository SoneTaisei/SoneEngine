#include "SpriteAnimationEditor.h"
#include <filesystem>
#include <algorithm>
#include "Graphics/TextureManager.h"

#ifdef USE_IMGUI
#include "imgui.h"
#endif

SpriteAnimationEditor::SpriteAnimationEditor() {
}

void SpriteAnimationEditor::Initialize() {
    RefreshFileList();
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
                    // スラッシュ区切りの相対パスに統一
                    std::string p = entry.path().generic_string();
                    fileList_.push_back(p);
                }
            }
        }
    }

    std::sort(fileList_.begin(), fileList_.end());

    // 最初のファイルがあれば自動選択してロード
    if (!fileList_.empty()) {
        selectedFileIndex_ = 0;
        loadedTextureHandle_ = TextureManager::GetInstance()->Load(fileList_[0]);
        const D3D12_RESOURCE_DESC resDesc = TextureManager::GetInstance()->GetResourceDesc(loadedTextureHandle_);
        textureBaseSize_ = { static_cast<float>(resDesc.Width), static_cast<float>(resDesc.Height) };
    }
}

void SpriteAnimationEditor::Update(float deltaTime) {
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
void SpriteAnimationEditor::DrawUI(bool* pOpen) {
    if (!pOpen || !*pOpen) return;

    if (ImGui::Begin("スプライトアニメーション", pOpen)) {
        ImGui::Text("【対象フォルダ】: %s", targetDirectory_.c_str());
        ImGui::SameLine();
        if (ImGui::Button("一覧を更新 (Refresh)")) {
            RefreshFileList();
        }

        ImGui::Separator();

        // ファイル選択コンボボックス
        if (fileList_.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "フォルダ内に画像ファイルがありません。");
            ImGui::TextWrapped("'%s' フォルダにスプライトシート（PNGやGIFなど）を配置して「一覧を更新」を押してください。", targetDirectory_.c_str());
            ImGui::End();
            return;
        }

        std::string currentFileName = "";
        if (selectedFileIndex_ >= 0 && selectedFileIndex_ < static_cast<int>(fileList_.size())) {
            currentFileName = std::filesystem::path(fileList_[selectedFileIndex_]).filename().string();
        }

        if (ImGui::BeginCombo("画像ファイル", currentFileName.c_str())) {
            for (int i = 0; i < static_cast<int>(fileList_.size()); ++i) {
                std::string fname = std::filesystem::path(fileList_[i]).filename().string();
                bool isSelected = (selectedFileIndex_ == i);
                if (ImGui::Selectable(fname.c_str(), isSelected)) {
                    if (selectedFileIndex_ != i) {
                        selectedFileIndex_ = i;
                        loadedTextureHandle_ = TextureManager::GetInstance()->Load(fileList_[i]);
                        const D3D12_RESOURCE_DESC resDesc = TextureManager::GetInstance()->GetResourceDesc(loadedTextureHandle_);
                        textureBaseSize_ = { static_cast<float>(resDesc.Width), static_cast<float>(resDesc.Height) };
                        currentFrame_ = 0;
                        animTimer_ = 0.0f;
                    }
                }
                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        // テクスチャ全体情報
        ImGui::Text("画像解像度: %.0f x %.0f px", textureBaseSize_.x, textureBaseSize_.y);

        ImGui::Spacing();
        ImGui::SeparatorText("パターンA：グリッド分割設定");

        if (ImGui::SliderInt("横分割数 (Columns)", &columns_, 1, 32)) {
            if (totalFrames_ > columns_ * rows_) totalFrames_ = columns_ * rows_;
        }
        if (ImGui::SliderInt("縦分割数 (Rows)", &rows_, 1, 32)) {
            if (totalFrames_ > columns_ * rows_) totalFrames_ = columns_ * rows_;
        }

        int maxPossibleFrames = columns_ * rows_;
        if (totalFrames_ <= 0 || totalFrames_ > maxPossibleFrames) {
            totalFrames_ = maxPossibleFrames;
        }
        ImGui::SliderInt("総コマ数 (TotalFrames)", &totalFrames_, 1, maxPossibleFrames);

        // 1コマあたりのピクセルサイズ
        float frameW = (columns_ > 0 && textureBaseSize_.x > 0.0f) ? (textureBaseSize_.x / static_cast<float>(columns_)) : 0.0f;
        float frameH = (rows_ > 0 && textureBaseSize_.y > 0.0f) ? (textureBaseSize_.y / static_cast<float>(rows_)) : 0.0f;
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.4f, 1.0f), "1コマのサイズ: %.1f x %.1f px (計 %d コマ)", frameW, frameH, totalFrames_);

        ImGui::Spacing();
        ImGui::SeparatorText("再生設定");

        ImGui::SliderFloat("再生速度 (FPS)", &fps_, 1.0f, 60.0f, "%.1f FPS");
        ImGui::Checkbox("ループ再生 (Loop)", &isLoop_);

        // 再生コントロールボタン
        if (isPlaying_) {
            if (ImGui::Button("⏸ 一時停止")) {
                isPlaying_ = false;
            }
        } else {
            if (ImGui::Button("▶ 再生")) {
                isPlaying_ = true;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("⏹ 停止")) {
            isPlaying_ = false;
            currentFrame_ = 0;
            animTimer_ = 0.0f;
        }

        // シークバー
        if (ImGui::SliderInt("シークバー (Frame)", &currentFrame_, 0, totalFrames_ - 1)) {
            animTimer_ = 0.0f;
        }

        ImGui::Spacing();
        ImGui::SeparatorText("プレビュー表示");

        ImGui::SliderFloat("プレビュースケール", &previewScale_, 0.25f, 4.0f, "%.2fx");

        // UV切り抜きプレビュー
        if (loadedTextureHandle_ != 0 && textureBaseSize_.x > 0.0f && textureBaseSize_.y > 0.0f) {
            int col = currentFrame_ % columns_;
            int row = currentFrame_ / columns_;

            ImVec2 uv0 = ImVec2((col * frameW) / textureBaseSize_.x, (row * frameH) / textureBaseSize_.y);
            ImVec2 uv1 = ImVec2(((col + 1) * frameW) / textureBaseSize_.x, ((row + 1) * frameH) / textureBaseSize_.y);

            ImVec2 previewSize = ImVec2(frameW * previewScale_, frameH * previewScale_);
            D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = TextureManager::GetInstance()->GetGpuHandle(loadedTextureHandle_);

            ImGui::Text("フレーム: %d / %d", currentFrame_ + 1, totalFrames_);
            ImGui::Image(
                (ImTextureID)gpuHandle.ptr,
                previewSize,
                uv0,
                uv1,
                ImVec4(1.0f, 1.0f, 1.0f, 1.0f),
                ImVec4(0.4f, 0.4f, 0.4f, 1.0f) // 境界線
            );
        }

        ImGui::Spacing();
        ImGui::Separator();

        // コードコピー用ボタン
        if (ImGui::Button("📋 C++コードをクリップボードにコピー")) {
            if (selectedFileIndex_ >= 0 && selectedFileIndex_ < static_cast<int>(fileList_.size())) {
                char codeBuf[512];
                snprintf(
                    codeBuf,
                    sizeof(codeBuf),
                    "// スプライトアニメーション設定\n"
                    "uint32_t texHandle = TextureManager::GetInstance()->Load(\"%s\");\n"
                    "tutorialSprite->Initialize(spriteCommon, texHandle);\n"
                    "tutorialSprite->SetAnimationGrid(%d, %d, %.1ff, %s, %d);\n"
                    "tutorialSprite->SetSize({ %.1ff, %.1ff });\n"
                    "tutorialSprite->PlayAnimation();\n",
                    fileList_[selectedFileIndex_].c_str(),
                    columns_,
                    rows_,
                    fps_,
                    isLoop_ ? "true" : "false",
                    totalFrames_,
                    frameW,
                    frameH
                );
                ImGui::SetClipboardText(codeBuf);
            }
        }
    }
    ImGui::End();
}
#endif
