#include "PostEffectEditor.h"
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <nlohmann/json.hpp>

PostEffectItem::PostEffectItem(const std::string& n, PostEffectShaderType type)
    : name(n), shaderType(type) {
    params.vignetteColor[0] = 0.0f;
    params.vignetteColor[1] = 0.0f;
    params.vignetteColor[2] = 0.0f;
    params.vignetteColor[3] = 1.0f;
    params.vignetteScale = 16.0f;
    params.vignettePower = 0.8f;
    params.enableVignette = 1;

    params.blurType = 1;
    params.boxBlurKernelSize = 1;
    params.boxBlurStrength = 1.0f;
    params.gaussianSigma = 2.0f;

    params.enableRadialBlur = 1;
    params.radialBlurCenter[0] = 0.5f;
    params.radialBlurCenter[1] = 0.5f;
    params.radialBlurWidth = 0.02f;
    params.radialBlurSamples = 10;

    params.enableDissolve = 1;
    params.dissolveThreshold = 0.0f;
    params.dissolveEdgeWidth = 0.03f;
    params.dissolveEdgeColor[0] = 1.0f;
    params.dissolveEdgeColor[1] = 0.4f;
    params.dissolveEdgeColor[2] = 0.3f;
    params.dissolveBgColor[0] = 0.0f;
    params.dissolveBgColor[1] = 0.0f;
    params.dissolveBgColor[2] = 0.0f;

    params.enableNoise = 1;
    params.noiseStrength = 0.3f;
    params.noiseScale = 128.0f;
    params.noiseBlendMode = 1;

    params.enableIris = 1;
    params.irisCenter[0] = 0.5f;
    params.irisCenter[1] = 0.5f;
    params.irisRadius = 0.8f;
    params.irisSmoothness = 0.02f;
    params.isIrisIn = 0;
    params.irisAspectRatio = 16.0f / 9.0f;
    params.irisMaskColor[0] = 0.0f;
    params.irisMaskColor[1] = 0.0f;
    params.irisMaskColor[2] = 0.0f;
    params.irisMaskColor[3] = 1.0f;

    params.grayscaleStrength = 1.0f;
    params.sepiaStrength = 0.8f;
}

PostEffectEditor::PostEffectEditor() {
    postEffects_.emplace_back("DefaultPostEffect", PostEffectShaderType::Sepia);
}

void PostEffectEditor::Initialize() {
    ScanPostEffectFiles();
    if (availableFiles_.empty()) {
        SaveToFile("resources/json/shared/PostEffect/post_effect_config.json");
        ScanPostEffectFiles();
    } else {
        LoadFromFile(currentFilePath_);
    }
}

void PostEffectEditor::Update(float deltaTime) {
    if (statusMessageTimer_ > 0.0f) {
        statusMessageTimer_ -= deltaTime;
        if (statusMessageTimer_ <= 0.0f) {
            statusMessage_ = "";
        }
    }
    ApplyToDirectXCommon();
}

void PostEffectEditor::ApplyToDirectXCommon() {
    auto dxCommon = DirectXCommon::GetInstance();
    if (!dxCommon) return;

    // 深度ベースアウトラインは常時デフォルトで有効
    dxCommon->SetDepthBasedOutlineEnabled(true);

    // ポストエフェクト全体（パス1:アウトライン -> パス2:コンポジット）は有効のままにし、
    // パラメータをリセットしてアウトラインのみを単体描画できるようにする
    dxCommon->SetPostEffectEnabled(true);
    dxCommon->SetPostEffect(DirectXCommon::PostEffect::kComposite);

    auto target = dxCommon->GetCompositeParamsData();
    if (!target) return;

    // 一旦全パラメータをクリーン（OFF）にする
    target->grayscaleStrength = 0.0f;
    target->sepiaStrength = 0.0f;
    target->enableVignette = 0;
    target->blurType = 0;
    target->enableRadialBlur = 0;
    target->enableDissolve = 0;
    target->enableNoise = 0;
    target->enableIris = 0;

    // 有効なポストエフェクトが1つでもあるかチェック
    bool anyEnabled = false;
    for (const auto& item : postEffects_) {
        if (item.enabled) {
            anyEnabled = true;
            break;
        }
    }

    if (!anyEnabled) {
        // ポストエフェクトが全OFFの場合は、コンポジットパラメータがすべて0なので、
        // パス1の深度ベースアウトライン結果がそのまま画面に出力される
        return;
    }

    // 有効（enabled）なすべてのポストエフェクトを順番に重ね合わせる
    for (const auto& item : postEffects_) {
        if (!item.enabled) continue;

        switch (item.shaderType) {
        case PostEffectShaderType::Grayscale:
            target->grayscaleStrength = (std::max)(target->grayscaleStrength, item.params.grayscaleStrength);
            break;

        case PostEffectShaderType::Sepia:
            target->sepiaStrength = (std::max)(target->sepiaStrength, item.params.sepiaStrength);
            break;

        case PostEffectShaderType::Vignette:
            if (item.params.enableVignette) {
                target->enableVignette = 1;
                target->vignetteScale = item.params.vignetteScale;
                target->vignettePower = item.params.vignettePower;
                for (int i = 0; i < 4; ++i) target->vignetteColor[i] = item.params.vignetteColor[i];
            }
            break;

        case PostEffectShaderType::Blur:
            if (item.params.blurType != 0) {
                target->blurType = item.params.blurType;
                target->boxBlurKernelSize = item.params.boxBlurKernelSize;
                target->boxBlurStrength = item.params.boxBlurStrength;
                target->gaussianSigma = item.params.gaussianSigma;
            }
            break;

        case PostEffectShaderType::RadialBlur:
            if (item.params.enableRadialBlur) {
                target->enableRadialBlur = 1;
                target->radialBlurCenter[0] = item.params.radialBlurCenter[0];
                target->radialBlurCenter[1] = item.params.radialBlurCenter[1];
                target->radialBlurWidth = item.params.radialBlurWidth;
                target->radialBlurSamples = item.params.radialBlurSamples;
            }
            break;

        case PostEffectShaderType::Dissolve:
            if (item.params.enableDissolve) {
                target->enableDissolve = 1;
                target->dissolveThreshold = item.params.dissolveThreshold;
                target->dissolveEdgeWidth = item.params.dissolveEdgeWidth;
                for (int i = 0; i < 3; ++i) {
                    target->dissolveEdgeColor[i] = item.params.dissolveEdgeColor[i];
                    target->dissolveBgColor[i] = item.params.dissolveBgColor[i];
                }
            }
            break;

        case PostEffectShaderType::Noise:
            if (item.params.enableNoise) {
                target->enableNoise = 1;
                target->noiseStrength = item.params.noiseStrength;
                target->noiseScale = item.params.noiseScale;
                target->noiseBlendMode = item.params.noiseBlendMode;
            }
            break;

        case PostEffectShaderType::Iris:
            if (item.params.enableIris) {
                target->enableIris = 1;
                target->irisCenter[0] = item.params.irisCenter[0];
                target->irisCenter[1] = item.params.irisCenter[1];
                target->irisRadius = item.params.irisRadius;
                target->irisSmoothness = item.params.irisSmoothness;
                target->isIrisIn = item.params.isIrisIn;
                for (int i = 0; i < 4; ++i) target->irisMaskColor[i] = item.params.irisMaskColor[i];
            }
            break;

        case PostEffectShaderType::Composite:
            if (item.params.grayscaleStrength > 0.0f) target->grayscaleStrength = (std::max)(target->grayscaleStrength, item.params.grayscaleStrength);
            if (item.params.sepiaStrength > 0.0f) target->sepiaStrength = (std::max)(target->sepiaStrength, item.params.sepiaStrength);
            if (item.params.enableVignette) {
                target->enableVignette = 1;
                target->vignetteScale = item.params.vignetteScale;
                target->vignettePower = item.params.vignettePower;
                for (int i = 0; i < 4; ++i) target->vignetteColor[i] = item.params.vignetteColor[i];
            }
            if (item.params.blurType != 0) {
                target->blurType = item.params.blurType;
                target->boxBlurKernelSize = item.params.boxBlurKernelSize;
                target->boxBlurStrength = item.params.boxBlurStrength;
                target->gaussianSigma = item.params.gaussianSigma;
            }
            if (item.params.enableRadialBlur) {
                target->enableRadialBlur = 1;
                target->radialBlurCenter[0] = item.params.radialBlurCenter[0];
                target->radialBlurCenter[1] = item.params.radialBlurCenter[1];
                target->radialBlurWidth = item.params.radialBlurWidth;
                target->radialBlurSamples = item.params.radialBlurSamples;
            }
            if (item.params.enableDissolve) {
                target->enableDissolve = 1;
                target->dissolveThreshold = item.params.dissolveThreshold;
                target->dissolveEdgeWidth = item.params.dissolveEdgeWidth;
                for (int i = 0; i < 3; ++i) {
                    target->dissolveEdgeColor[i] = item.params.dissolveEdgeColor[i];
                    target->dissolveBgColor[i] = item.params.dissolveBgColor[i];
                }
            }
            if (item.params.enableNoise) {
                target->enableNoise = 1;
                target->noiseStrength = item.params.noiseStrength;
                target->noiseScale = item.params.noiseScale;
                target->noiseBlendMode = item.params.noiseBlendMode;
            }
            if (item.params.enableIris) {
                target->enableIris = 1;
                target->irisCenter[0] = item.params.irisCenter[0];
                target->irisCenter[1] = item.params.irisCenter[1];
                target->irisRadius = item.params.irisRadius;
                target->irisSmoothness = item.params.irisSmoothness;
                target->isIrisIn = item.params.isIrisIn;
                for (int i = 0; i < 4; ++i) target->irisMaskColor[i] = item.params.irisMaskColor[i];
            }
            break;

        default:
            break;
        }
    }
}

void PostEffectEditor::AddPostEffect(const std::string& name) {
    std::string newName = name;
    if (newName.empty()) {
        newName = "PostEffect_" + std::to_string(postEffects_.size() + 1);
    }
    postEffects_.emplace_back(newName, PostEffectShaderType::Sepia);
    selectedPostEffectIndex_ = static_cast<int>(postEffects_.size()) - 1;
    shouldFocusWindow_ = true;
    if (onSelectCallback_) onSelectCallback_();
    SetStatusMessage("ポストエフェクトを追加しました: " + newName);
}

void PostEffectEditor::DuplicatePostEffect(int index) {
    if (index >= 0 && index < static_cast<int>(postEffects_.size())) {
        PostEffectItem copy = postEffects_[index];
        copy.name += "_Copy";
        postEffects_.insert(postEffects_.begin() + index + 1, copy);
        selectedPostEffectIndex_ = index + 1;
        shouldFocusWindow_ = true;
        if (onSelectCallback_) onSelectCallback_();
        SetStatusMessage("ポストエフェクトを複製しました: " + copy.name);
    }
}

void PostEffectEditor::RemovePostEffect(int index) {
    if (index >= 0 && index < static_cast<int>(postEffects_.size())) {
        std::string deletedName = postEffects_[index].name;
        postEffects_.erase(postEffects_.begin() + index);
        if (postEffects_.empty()) {
            selectedPostEffectIndex_ = -1;
        } else if (selectedPostEffectIndex_ >= static_cast<int>(postEffects_.size())) {
            selectedPostEffectIndex_ = static_cast<int>(postEffects_.size()) - 1;
        }
        SetStatusMessage("削除しました: " + deletedName);
    }
}

void PostEffectEditor::ClearAll() {
    postEffects_.clear();
    selectedPostEffectIndex_ = -1;
}

PostEffectItem* PostEffectEditor::GetSelectedPostEffect() {
    if (selectedPostEffectIndex_ >= 0 && selectedPostEffectIndex_ < static_cast<int>(postEffects_.size())) {
        return &postEffects_[selectedPostEffectIndex_];
    }
    return nullptr;
}

const PostEffectItem* PostEffectEditor::GetSelectedPostEffect() const {
    if (selectedPostEffectIndex_ >= 0 && selectedPostEffectIndex_ < static_cast<int>(postEffects_.size())) {
        return &postEffects_[selectedPostEffectIndex_];
    }
    return nullptr;
}

void PostEffectEditor::SetSelectedPostEffectIndex(int idx) {
    if (idx >= 0 && idx < static_cast<int>(postEffects_.size())) {
        selectedPostEffectIndex_ = idx;
    }
}

std::string PostEffectEditor::ResolveFilePath(const std::string& fileNameOrPath) const {
    if (fileNameOrPath.empty()) {
        return currentFilePath_;
    }
    std::string path = fileNameOrPath;
    std::replace(path.begin(), path.end(), '\\', '/');
    if (path.find('/') == std::string::npos) {
        path = "resources/json/shared/PostEffect/" + path;
    }
    if (path.length() < 5 || path.substr(path.length() - 5) != ".json") {
        path += ".json";
    }
    return path;
}

std::string PostEffectEditor::GetCurrentFileName() const {
    return std::filesystem::path(currentFilePath_).filename().string();
}

void PostEffectEditor::SetCurrentFilePath(const std::string& path) {
    currentFilePath_ = ResolveFilePath(path);
    strcpy_s(saveFileNameBuf_, sizeof(saveFileNameBuf_), GetCurrentFileName().c_str());
    ScanPostEffectFiles();
}

std::string PostEffectEditor::StripJsonExtension(const std::string& filename) {
    if (filename.length() >= 5 && filename.substr(filename.length() - 5) == ".json") {
        return filename.substr(0, filename.length() - 5);
    }
    return filename;
}

void PostEffectEditor::ScanPostEffectFiles() {
    availableFiles_.clear();
    const std::string dir = "resources/json/shared/PostEffect";
    try {
        if (!std::filesystem::exists(dir)) {
            std::filesystem::create_directories(dir);
        }
        for (const auto& entry : std::filesystem::directory_iterator(dir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json") {
                availableFiles_.push_back(entry.path().filename().string());
            }
        }
        std::sort(availableFiles_.begin(), availableFiles_.end());
    } catch (...) {}
}

bool PostEffectEditor::SaveToFile(const std::string& filePath) {
    std::string path = ResolveFilePath(filePath);
    try {
        std::filesystem::create_directories(std::filesystem::path(path).parent_path());
        nlohmann::json root;
        root["name"] = StripJsonExtension(std::filesystem::path(path).filename().string());
        root["selectedIndex"] = selectedPostEffectIndex_;

        nlohmann::json effectsArray = nlohmann::json::array();
        for (const auto& item : postEffects_) {
            nlohmann::json j;
            j["name"] = item.name;
            j["enabled"] = item.enabled;
            j["shaderType"] = static_cast<int>(item.shaderType);

            nlohmann::json params;
            params["grayscale"] = {
                {"strength", item.params.grayscaleStrength}
            };
            params["sepia"] = {
                {"strength", item.params.sepiaStrength}
            };
            params["vignette"] = {
                {"enabled", item.params.enableVignette != 0},
                {"color", {item.params.vignetteColor[0], item.params.vignetteColor[1], item.params.vignetteColor[2], item.params.vignetteColor[3]}},
                {"scale", item.params.vignetteScale},
                {"power", item.params.vignettePower}
            };
            params["blur"] = {
                {"type", item.params.blurType},
                {"boxKernelSize", item.params.boxBlurKernelSize},
                {"boxStrength", item.params.boxBlurStrength},
                {"gaussianSigma", item.params.gaussianSigma}
            };
            params["radialBlur"] = {
                {"enabled", item.params.enableRadialBlur != 0},
                {"center", {item.params.radialBlurCenter[0], item.params.radialBlurCenter[1]}},
                {"width", item.params.radialBlurWidth},
                {"samples", item.params.radialBlurSamples}
            };
            params["dissolve"] = {
                {"enabled", item.params.enableDissolve != 0},
                {"threshold", item.params.dissolveThreshold},
                {"edgeWidth", item.params.dissolveEdgeWidth},
                {"edgeColor", {item.params.dissolveEdgeColor[0], item.params.dissolveEdgeColor[1], item.params.dissolveEdgeColor[2]}},
                {"bgColor", {item.params.dissolveBgColor[0], item.params.dissolveBgColor[1], item.params.dissolveBgColor[2]}}
            };
            params["noise"] = {
                {"enabled", item.params.enableNoise != 0},
                {"strength", item.params.noiseStrength},
                {"scale", item.params.noiseScale},
                {"blendMode", item.params.noiseBlendMode}
            };
            params["iris"] = {
                {"enabled", item.params.enableIris != 0},
                {"center", {item.params.irisCenter[0], item.params.irisCenter[1]}},
                {"radius", item.params.irisRadius},
                {"smoothness", item.params.irisSmoothness},
                {"isIrisIn", item.params.isIrisIn},
                {"maskColor", {item.params.irisMaskColor[0], item.params.irisMaskColor[1], item.params.irisMaskColor[2], item.params.irisMaskColor[3]}}
            };

            j["params"] = params;
            effectsArray.push_back(j);
        }
        root["postEffects"] = effectsArray;

        std::ofstream ofs(path);
        if (!ofs.is_open()) {
            SetStatusMessage("保存失敗: ファイルを開けませんでした: " + path);
            return false;
        }
        ofs << root.dump(4);
        ofs.close();

        currentFilePath_ = path;
        strcpy_s(saveFileNameBuf_, sizeof(saveFileNameBuf_), GetCurrentFileName().c_str());
        ScanPostEffectFiles();
        if (onFileChangedCallback_) onFileChangedCallback_();
        SetStatusMessage("保存しました: " + GetCurrentFileName());
        return true;
    } catch (...) {
        SetStatusMessage("保存エラーが発生しました");
        return false;
    }
}

bool PostEffectEditor::LoadFromFile(const std::string& filePath) {
    std::string path = ResolveFilePath(filePath);
    try {
        std::ifstream ifs(path);
        if (!ifs.is_open()) {
            SetStatusMessage("読み込み失敗: ファイルが見つかりません: " + path);
            return false;
        }
        nlohmann::json root;
        ifs >> root;
        ifs.close();

        if (root.contains("postEffects") && root["postEffects"].is_array()) {
            postEffects_.clear();
            for (const auto& j : root["postEffects"]) {
                PostEffectItem item;
                item.name = j.value("name", "PostEffect");
                item.enabled = j.value("enabled", true);
                int rawType = j.value("shaderType", static_cast<int>(PostEffectShaderType::Sepia));
                if (rawType < 0 || rawType >= static_cast<int>(PostEffectShaderType::Count)) {
                    item.shaderType = PostEffectShaderType::Sepia;
                } else {
                    item.shaderType = static_cast<PostEffectShaderType>(rawType);
                }

                // params または 旧フォーマット shaders から読み込み
                const std::string key = j.contains("params") ? "params" : (j.contains("shaders") ? "shaders" : "");
                if (!key.empty()) {
                    const auto& s = j[key];
                    if (s.contains("grayscale")) {
                        item.params.grayscaleStrength = s["grayscale"].value("strength", 1.0f);
                    }
                    if (s.contains("sepia")) {
                        item.params.sepiaStrength = s["sepia"].value("strength", 0.8f);
                    }
                    if (s.contains("vignette")) {
                        item.params.enableVignette = s["vignette"].value("enabled", true) ? 1 : 0;
                        item.params.vignetteScale = s["vignette"].value("scale", 16.0f);
                        item.params.vignettePower = s["vignette"].value("power", 0.8f);
                        if (s["vignette"].contains("color") && s["vignette"]["color"].is_array()) {
                            for (size_t c = 0; c < 4 && c < s["vignette"]["color"].size(); ++c) {
                                item.params.vignetteColor[c] = s["vignette"]["color"][c].get<float>();
                            }
                        }
                    }
                    if (s.contains("blur")) {
                        item.params.blurType = s["blur"].value("type", 1);
                        item.params.boxBlurKernelSize = s["blur"].value("boxKernelSize", 1);
                        item.params.boxBlurStrength = s["blur"].value("boxStrength", 1.0f);
                        item.params.gaussianSigma = s["blur"].value("gaussianSigma", 2.0f);
                    }
                    if (s.contains("radialBlur")) {
                        item.params.enableRadialBlur = s["radialBlur"].value("enabled", true) ? 1 : 0;
                        item.params.radialBlurWidth = s["radialBlur"].value("width", 0.02f);
                        item.params.radialBlurSamples = s["radialBlur"].value("samples", 10);
                        if (s["radialBlur"].contains("center") && s["radialBlur"]["center"].is_array()) {
                            for (size_t c = 0; c < 2 && c < s["radialBlur"]["center"].size(); ++c) {
                                item.params.radialBlurCenter[c] = s["radialBlur"]["center"][c].get<float>();
                            }
                        }
                    }
                    if (s.contains("dissolve")) {
                        item.params.enableDissolve = s["dissolve"].value("enabled", true) ? 1 : 0;
                        item.params.dissolveThreshold = s["dissolve"].value("threshold", 0.0f);
                        item.params.dissolveEdgeWidth = s["dissolve"].value("edgeWidth", 0.03f);
                        if (s["dissolve"].contains("edgeColor") && s["dissolve"]["edgeColor"].is_array()) {
                            for (size_t c = 0; c < 3 && c < s["dissolve"]["edgeColor"].size(); ++c) {
                                item.params.dissolveEdgeColor[c] = s["dissolve"]["edgeColor"][c].get<float>();
                            }
                        }
                        if (s["dissolve"].contains("bgColor") && s["dissolve"]["bgColor"].is_array()) {
                            for (size_t c = 0; c < 3 && c < s["dissolve"]["bgColor"].size(); ++c) {
                                item.params.dissolveBgColor[c] = s["dissolve"]["bgColor"][c].get<float>();
                            }
                        }
                    }
                    if (s.contains("noise")) {
                        item.params.enableNoise = s["noise"].value("enabled", true) ? 1 : 0;
                        item.params.noiseStrength = s["noise"].value("strength", 0.3f);
                        item.params.noiseScale = s["noise"].value("scale", 128.0f);
                        item.params.noiseBlendMode = s["noise"].value("blendMode", 1);
                    }
                    if (s.contains("iris")) {
                        item.params.enableIris = s["iris"].value("enabled", true) ? 1 : 0;
                        item.params.irisRadius = s["iris"].value("radius", 0.8f);
                        item.params.irisSmoothness = s["iris"].value("smoothness", 0.02f);
                        item.params.isIrisIn = s["iris"].value("isIrisIn", 0);
                        if (s["iris"].contains("center") && s["iris"]["center"].is_array()) {
                            for (size_t c = 0; c < 2 && c < s["iris"]["center"].size(); ++c) {
                                item.params.irisCenter[c] = s["iris"]["center"][c].get<float>();
                            }
                        }
                        if (s["iris"].contains("maskColor") && s["iris"]["maskColor"].is_array()) {
                            for (size_t c = 0; c < 4 && c < s["iris"]["maskColor"].size(); ++c) {
                                item.params.irisMaskColor[c] = s["iris"]["maskColor"][c].get<float>();
                            }
                        }
                    }
                }
                postEffects_.push_back(item);
            }
            if (postEffects_.empty()) {
                selectedPostEffectIndex_ = -1;
            } else {
                selectedPostEffectIndex_ = root.value("selectedIndex", 0);
                if (selectedPostEffectIndex_ < 0 || selectedPostEffectIndex_ >= static_cast<int>(postEffects_.size())) {
                    selectedPostEffectIndex_ = 0;
                }
            }
        }

        currentFilePath_ = path;
        strcpy_s(saveFileNameBuf_, sizeof(saveFileNameBuf_), GetCurrentFileName().c_str());
        ScanPostEffectFiles();
        ApplyToDirectXCommon();
        if (onFileChangedCallback_) onFileChangedCallback_();
        SetStatusMessage("読み込みました: " + GetCurrentFileName());
        return true;
    } catch (...) {
        SetStatusMessage("JSON読み込みエラーが発生しました");
        return false;
    }
}

bool PostEffectEditor::DeleteFile(const std::string& filePath) {
    std::string path = ResolveFilePath(filePath);
    try {
        if (std::filesystem::exists(path)) {
            std::filesystem::remove(path);
        }
        ScanPostEffectFiles();
        if (!availableFiles_.empty()) {
            LoadFromFile(availableFiles_[0]);
        } else {
            ClearAll();
            currentFilePath_ = "resources/json/shared/PostEffect/post_effect_config.json";
            strcpy_s(saveFileNameBuf_, sizeof(saveFileNameBuf_), GetCurrentFileName().c_str());
            SaveToFile();
        }
        SetStatusMessage("ファイルを削除しました: " + StripJsonExtension(std::filesystem::path(path).filename().string()));
        return true;
    } catch (...) {
        SetStatusMessage("ファイル削除エラーが発生しました");
        return false;
    }
}

#ifdef USE_IMGUI

static const char* GetShaderTypeName(PostEffectShaderType type) {
    switch (type) {
    case PostEffectShaderType::Grayscale:  return "グレースケール";
    case PostEffectShaderType::Sepia:      return "セピア";
    case PostEffectShaderType::Vignette:   return "ヴィニエット";
    case PostEffectShaderType::Blur:       return "ブラー";
    case PostEffectShaderType::RadialBlur: return "ラジアルブラー";
    case PostEffectShaderType::Dissolve:   return "ディゾルブ";
    case PostEffectShaderType::Noise:      return "ノイズ";
    case PostEffectShaderType::Iris:       return "アイリス";
    case PostEffectShaderType::Composite:  return "複合";
    default: return "";
    }
}

void PostEffectEditor::DrawHierarchy() {
    bool headerOpen = ImGui::CollapsingHeader("ポストエフェクト (Post Effects)", ImGuiTreeNodeFlags_DefaultOpen);
    ImGui::SameLine(ImGui::GetWindowWidth() - 35);
    if (ImGui::SmallButton("+##AddPEHierarchy")) {
        AddPostEffect();
        shouldFocusWindow_ = true;
        if (onSelectCallback_) onSelectCallback_();
    }
    if (headerOpen) {
        int indexToDelete = -1;
        int indexToDuplicate = -1;

        if (postEffects_.empty()) {
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "  (ポストエフェクトなし)");
            ImGui::Spacing();
        }

        // ヒエラルキーウィンドウ等にフォーカスがある状態で Delete / Backspace キーが押されたら選択中項目を削除
        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
            (ImGui::IsKeyPressed(ImGuiKey_Delete) || ImGui::IsKeyPressed(ImGuiKey_Backspace))) {
            if (selectedPostEffectIndex_ >= 0 && selectedPostEffectIndex_ < static_cast<int>(postEffects_.size())) {
                indexToDelete = selectedPostEffectIndex_;
            }
        }

        for (int i = 0; i < static_cast<int>(postEffects_.size()); ++i) {
            auto& item = postEffects_[i];
            bool isSelected = (selectedPostEffectIndex_ == i);

            ImGui::PushID(i);

            // 有効化トグル
            bool enabled = item.enabled;
            if (ImGui::Checkbox("##enabled", &enabled)) {
                item.enabled = enabled;
            }
            ImGui::SameLine();

            // 名前選択 (例: [ON] MySepia (セピア))
            std::string label = (item.enabled ? "[ON] " : "[OFF] ") + item.name + " (" + GetShaderTypeName(item.shaderType) + ")";
            float availWidth = ImGui::GetContentRegionAvail().x - 26.0f;
            if (availWidth < 50.0f) availWidth = 50.0f;
            if (ImGui::Selectable(label.c_str(), isSelected, 0, ImVec2(availWidth, 0))) {
                selectedPostEffectIndex_ = i;
                shouldFocusWindow_ = true;
                if (onSelectCallback_) onSelectCallback_();
            }

            // アイテム選択中に Delete キーが押されたら削除
            if (isSelected && (ImGui::IsKeyPressed(ImGuiKey_Delete) || ImGui::IsKeyPressed(ImGuiKey_Backspace))) {
                indexToDelete = i;
            }

            // 右端に「×」削除ボタン
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.2f, 0.2f, 0.4f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.9f, 0.1f, 0.1f, 0.8f));
            if (ImGui::SmallButton("×##DelPE")) {
                indexToDelete = i;
            }
            ImGui::PopStyleColor(3);

            // 右クリックコンテキストメニュー（1個だけでも常に削除可能）
            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("複製 (Duplicate)")) {
                    indexToDuplicate = i;
                }
                if (ImGui::MenuItem("削除 (Delete)")) {
                    indexToDelete = i;
                }
                ImGui::EndPopup();
            }

            ImGui::PopID();
        }

        if (indexToDuplicate >= 0) {
            DuplicatePostEffect(indexToDuplicate);
        }
        if (indexToDelete >= 0) {
            RemovePostEffect(indexToDelete);
        }
    }
}

void PostEffectEditor::DrawUI(bool* pOpen) {
    (void)pOpen;

    // =========================================================================
    // 1. 最上部: 保存ボタン関連（ファイル名入力・保存・ファイル一覧）
    // =========================================================================
    ImGui::Text("ポストエフェクト設定 (PostEffect)");
    ImGui::Separator();
    ImGui::Spacing();

    // 既存ファイル選択コンボ（常に最新のフォルダ一覧を取得）
    ScanPostEffectFiles();
    std::string curFileName = GetCurrentFileName();
    const auto& fileList = availableFiles_;
    selectedFileComboIdx_ = -1;
    for (int i = 0; i < static_cast<int>(fileList.size()); ++i) {
        if (fileList[i] == curFileName) {
            selectedFileComboIdx_ = i;
            break;
        }
    }

    std::string comboPreview = (selectedFileComboIdx_ != -1) ? fileList[selectedFileComboIdx_] : (curFileName.empty() ? "ファイルを選択..." : curFileName);
    if (ImGui::BeginCombo("既存ファイル", comboPreview.c_str())) {
        for (int i = 0; i < static_cast<int>(fileList.size()); ++i) {
            bool isSelected = (selectedFileComboIdx_ == i);
            if (ImGui::Selectable(fileList[i].c_str(), isSelected)) {
                selectedFileComboIdx_ = i;
                strcpy_s(saveFileNameBuf_, sizeof(saveFileNameBuf_), fileList[i].c_str());
                LoadFromFile(fileList[i]);
            }
            if (isSelected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    // ファイル名入力
    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
    if (ImGui::InputTextWithHint("##saveFileName", "保存ファイル名 (例: post_effect.json)", saveFileNameBuf_, sizeof(saveFileNameBuf_), ImGuiInputTextFlags_EnterReturnsTrue)) {
        if (strlen(saveFileNameBuf_) > 0) {
            LoadFromFile(saveFileNameBuf_);
        }
    }
    ImGui::PopItemWidth();

    ImGui::Spacing();

    // 保存ボタン & 読み込みボタン & 削除ボタン
    if (ImGui::Button("保存 (Save)", ImVec2(90, 26))) {
        if (strlen(saveFileNameBuf_) > 0) {
            SaveToFile(saveFileNameBuf_);
        } else {
            SaveToFile();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("読み込み (Load)", ImVec2(100, 26))) {
        if (strlen(saveFileNameBuf_) > 0) {
            LoadFromFile(saveFileNameBuf_);
        } else {
            LoadFromFile();
        }
    }
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.85f, 0.3f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.6f, 0.1f, 0.1f, 1.0f));
    if (ImGui::Button("ファイルを削除", ImVec2(100, 26))) {
        ImGui::OpenPopup("DeletePostEffectConfirmPopup");
    }
    ImGui::PopStyleColor(3);

    // ステータスメッセージ表示
    if (!statusMessage_.empty() && statusMessageTimer_ > 0.0f) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.3f, 1.0f), "%s", statusMessage_.c_str());
    }

    // 削除確認ポップアップ
    if (ImGui::BeginPopupModal("DeletePostEffectConfirmPopup", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        std::string targetFile = GetCurrentFileName();
        ImGui::Text("本当にポストエフェクト設定ファイル '%s' を削除しますか？", targetFile.c_str());
        ImGui::Spacing();
        if (ImGui::Button("はい、削除します", ImVec2(140, 0))) {
            DeleteFile(targetFile);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("キャンセル", ImVec2(100, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // =========================================================================
    // 2. 「ポストエフェクトを追加」ボタン（保存ボタンの下）
    // =========================================================================
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.52f, 0.32f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.65f, 0.40f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.12f, 0.40f, 0.24f, 1.0f));
    if (ImGui::Button("＋ ポストエフェクトを追加", ImVec2(ImGui::GetContentRegionAvail().x, 30))) {
        AddPostEffect();
    }
    ImGui::PopStyleColor(3);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // =========================================================================
    // 3. 選択中ポストエフェクトの編集
    // =========================================================================
    // ポストエフェクトウィンドウ内で Delete キーが押された場合も削除
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
        (ImGui::IsKeyPressed(ImGuiKey_Delete) || ImGui::IsKeyPressed(ImGuiKey_Backspace))) {
        if (selectedPostEffectIndex_ >= 0 && selectedPostEffectIndex_ < static_cast<int>(postEffects_.size())) {
            RemovePostEffect(selectedPostEffectIndex_);
            return;
        }
    }

    PostEffectItem* currentItem = GetSelectedPostEffect();
    if (!currentItem) {
        if (postEffects_.empty()) {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "ポストエフェクトがありません。上の「＋ ポストエフェクトを追加」ボタンを押して作成してください。");
        } else {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "ポストエフェクトが選択されていません。ヒエラルキーから選択してください。");
        }
        return;
    }

    // 名前編集
    char nameBuf[128];
    strcpy_s(nameBuf, sizeof(nameBuf), currentItem->name.c_str());
    ImGui::Text("名前:");
    ImGui::SameLine();
    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
    if (ImGui::InputText("##effectName", nameBuf, sizeof(nameBuf))) {
        currentItem->name = nameBuf;
    }
    ImGui::PopItemWidth();

    // 有効化チェックボックス & 削除ボタン
    ImGui::Checkbox("このポストエフェクトを有効化", &currentItem->enabled);
    ImGui::SameLine(ImGui::GetWindowWidth() - 75.0f);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.3f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.1f, 0.1f, 1.0f));
    if (ImGui::Button("削除##DeleteBtnInInspector", ImVec2(60.0f, 0))) {
        RemovePostEffect(selectedPostEffectIndex_);
        ImGui::PopStyleColor(3);
        return;
    }
    ImGui::PopStyleColor(3);

    ImGui::Spacing();

    // =========================================================================
    // 4. 「シェーダーを選択」（選んだら即座にそのシェーダーを編集）
    // =========================================================================
    const char* shaderNames[] = {
        "グレースケール (Grayscale)",
        "セピア (Sepia)",
        "ヴィニエット (Vignette)",
        "ブラー (Blur)",
        "ラジアルブラー (Radial Blur)",
        "ディゾルブ (Dissolve)",
        "ノイズ (Noise)",
        "アイリス (Iris)",
        "複合 (Composite)"
    };

    int currentTypeIdx = static_cast<int>(currentItem->shaderType);
    ImGui::Text("シェーダーを選択:");
    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
    if (ImGui::Combo("##shaderSelect", &currentTypeIdx, shaderNames, IM_ARRAYSIZE(shaderNames))) {
        currentItem->shaderType = static_cast<PostEffectShaderType>(currentTypeIdx);
    }
    ImGui::PopItemWidth();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // =========================================================================
    // 5. 選択中シェーダーのパラメータ調整UI（選んだシェーダーをそのまま編集）
    // =========================================================================
    auto DrawFloatControl = [&](const char *label, float *val, float minVal, float maxVal, float speed = 0.005f) {
        ImGui::Text("%s", label);
        ImGui::PushID(label);
        ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
        ImGui::DragFloat("##drag", val, speed, minVal, maxVal, "%.3f");
        ImGui::PopItemWidth();
        ImGui::PopID();
    };

    auto DrawIntControl = [&](const char *label, int *val, int minVal, int maxVal, float speed = 0.05f) {
        ImGui::Text("%s", label);
        ImGui::PushID(label);
        ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
        ImGui::DragInt("##drag", val, speed, minVal, maxVal);
        ImGui::PopItemWidth();
        ImGui::PopID();
    };

    bool isAll = (currentItem->shaderType == PostEffectShaderType::Composite);

    // --- グレースケール ---
    if (currentItem->shaderType == PostEffectShaderType::Grayscale || isAll) {
        if (ImGui::CollapsingHeader("グレースケール設定 (Grayscale)", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Spacing();
            DrawFloatControl("グレースケール強度", &currentItem->params.grayscaleStrength, 0.0f, 1.0f);
            ImGui::Spacing();
        }
        ImGui::Spacing();
    }

    // --- セピア ---
    if (currentItem->shaderType == PostEffectShaderType::Sepia || isAll) {
        if (ImGui::CollapsingHeader("セピア設定 (Sepia)", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Spacing();
            DrawFloatControl("セピア強度", &currentItem->params.sepiaStrength, 0.0f, 1.0f);
            ImGui::Spacing();
        }
        ImGui::Spacing();
    }

    // --- ヴィニエット ---
    if (currentItem->shaderType == PostEffectShaderType::Vignette || isAll) {
        if (ImGui::CollapsingHeader("ヴィニエット設定 (Vignette)", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Spacing();
            bool enableVignette = (currentItem->params.enableVignette != 0);
            if (ImGui::Checkbox("ヴィニエットを有効化", &enableVignette)) {
                currentItem->params.enableVignette = enableVignette ? 1 : 0;
            }
            if (enableVignette) {
                ImGui::Text("ヴィニエット色");
                ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
                ImGui::ColorEdit4("##vignetteColor", currentItem->params.vignetteColor);
                ImGui::PopItemWidth();

                DrawFloatControl("ヴィニエットスケール", &currentItem->params.vignetteScale, 0.0f, 100.0f, 0.1f);
                DrawFloatControl("ヴィニエット強度 (Power)", &currentItem->params.vignettePower, 0.0f, 10.0f, 0.01f);
            }
            ImGui::Spacing();
        }
        ImGui::Spacing();
    }

    // --- ブラー ---
    if (currentItem->shaderType == PostEffectShaderType::Blur || isAll) {
        if (ImGui::CollapsingHeader("ブラー設定 (Blur)", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Spacing();
            ImGui::Text("ブラータイプ");
            ImGui::RadioButton("なし", &currentItem->params.blurType, 0);
            ImGui::SameLine();
            ImGui::RadioButton("ボックスブラー", &currentItem->params.blurType, 1);
            ImGui::SameLine();
            ImGui::RadioButton("ガウシアンブラー", &currentItem->params.blurType, 2);

            if (currentItem->params.blurType == 1) {
                ImGui::Spacing();
                DrawIntControl("カーネルサイズ", &currentItem->params.boxBlurKernelSize, 1, 5);
                DrawFloatControl("ブラー強度", &currentItem->params.boxBlurStrength, 0.0f, 1.0f);
            } else if (currentItem->params.blurType == 2) {
                ImGui::Spacing();
                DrawFloatControl("ガウシアンシグマ (Sigma)", &currentItem->params.gaussianSigma, 0.1f, 10.0f, 0.1f);
            }
            ImGui::Spacing();
        }
        ImGui::Spacing();
    }

    // --- ラジアルブラー ---
    if (currentItem->shaderType == PostEffectShaderType::RadialBlur || isAll) {
        if (ImGui::CollapsingHeader("ラジアルブラー設定 (Radial Blur)", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Spacing();
            bool enableRadial = (currentItem->params.enableRadialBlur != 0);
            if (ImGui::Checkbox("ラジアルブラーを有効化", &enableRadial)) {
                currentItem->params.enableRadialBlur = enableRadial ? 1 : 0;
            }
            if (enableRadial) {
                ImGui::Spacing();
                DrawFloatControl("ブラー幅 (Blur Width)", &currentItem->params.radialBlurWidth, 0.0f, 0.1f, 0.001f);
                ImGui::Spacing();
                DrawIntControl("サンプル数 (ブラー品質)", &currentItem->params.radialBlurSamples, 1, 30);
                ImGui::Spacing();

                ImGui::Text("中心位置 (クリック/ドラッグで調整)");
                ImGui::Spacing();

                float aspect = 1.0f;
                auto dxCommon = DirectXCommon::GetInstance();
                if (dxCommon) {
                    int32_t width = dxCommon->GetWindowWidth();
                    int32_t height = dxCommon->GetWindowHeight();
                    if (width > 0 && height > 0) {
                        aspect = (float)height / (float)width;
                    }
                }

                ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
                ImVec2 canvas_size = ImVec2(200.0f, 200.0f * aspect);

                ImGui::InvisibleButton("##canvas", canvas_size);
                bool is_active = ImGui::IsItemActive();

                if (is_active) {
                    ImVec2 mouse_pos = ImGui::GetIO().MousePos;
                    float x_uv = (mouse_pos.x - canvas_pos.x) / canvas_size.x;
                    float y_uv = (mouse_pos.y - canvas_pos.y) / canvas_size.y;
                    x_uv = (std::max)(0.0f, (std::min)(1.0f, x_uv));
                    y_uv = (std::max)(0.0f, (std::min)(1.0f, y_uv));
                    currentItem->params.radialBlurCenter[0] = x_uv;
                    currentItem->params.radialBlurCenter[1] = y_uv;
                }

                ImDrawList *draw_list = ImGui::GetWindowDrawList();
                draw_list->AddRectFilled(canvas_pos, ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y), IM_COL32(35, 35, 35, 255));
                draw_list->AddRect(canvas_pos, ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y), IM_COL32(80, 80, 80, 255));

                draw_list->AddLine(
                    ImVec2(canvas_pos.x, canvas_pos.y + canvas_size.y * 0.5f),
                    ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y * 0.5f),
                    IM_COL32(100, 100, 100, 100));
                draw_list->AddLine(
                    ImVec2(canvas_pos.x + canvas_size.x * 0.5f, canvas_pos.y),
                    ImVec2(canvas_pos.x + canvas_size.x * 0.5f, canvas_pos.y + canvas_size.y),
                    IM_COL32(100, 100, 100, 100));

                ImVec2 dot_pos = ImVec2(canvas_pos.x + currentItem->params.radialBlurCenter[0] * canvas_size.x, canvas_pos.y + currentItem->params.radialBlurCenter[1] * canvas_size.y);
                draw_list->AddCircleFilled(dot_pos, 6.0f, IM_COL32(255, 100, 100, 255));
                draw_list->AddCircle(dot_pos, 6.0f, IM_COL32(255, 255, 255, 255), 0, 1.5f);

                ImGui::Spacing();
                ImGui::Text("中心 UV: (%.3f, %.3f)", currentItem->params.radialBlurCenter[0], currentItem->params.radialBlurCenter[1]);

                ImGui::Spacing();
                ImGui::Text("数値手動入力");
                ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
                ImGui::DragFloat2("##center_input", currentItem->params.radialBlurCenter, 0.002f, 0.0f, 1.0f, "%.3f");
                ImGui::PopItemWidth();
            }
            ImGui::Spacing();
        }
        ImGui::Spacing();
    }

    // --- ディゾルブ ---
    if (currentItem->shaderType == PostEffectShaderType::Dissolve || isAll) {
        if (ImGui::CollapsingHeader("ディゾルブ設定 (Dissolve)", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Spacing();
            bool enableDissolve = (currentItem->params.enableDissolve != 0);
            if (ImGui::Checkbox("ディゾルブを有効化", &enableDissolve)) {
                currentItem->params.enableDissolve = enableDissolve ? 1 : 0;
            }
            if (enableDissolve) {
                ImGui::Spacing();
                DrawFloatControl("しきい値 (Threshold)", &currentItem->params.dissolveThreshold, 0.0f, 1.0f, 0.005f);
                ImGui::Spacing();
                DrawFloatControl("エッジ幅", &currentItem->params.dissolveEdgeWidth, 0.0f, 0.2f, 0.002f);
                ImGui::Spacing();

                ImGui::Text("エッジ色");
                ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
                ImGui::ColorEdit3("##dissolveEdgeColor", currentItem->params.dissolveEdgeColor);
                ImGui::PopItemWidth();
                ImGui::Spacing();

                ImGui::Text("背景色 (本体)");
                ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
                ImGui::ColorEdit3("##dissolveBgColor", currentItem->params.dissolveBgColor);
                ImGui::PopItemWidth();
            }
            ImGui::Spacing();
        }
        ImGui::Spacing();
    }

    // --- ノイズ ---
    if (currentItem->shaderType == PostEffectShaderType::Noise || isAll) {
        if (ImGui::CollapsingHeader("ノイズ設定 (Noise)", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Spacing();
            bool enableNoise = (currentItem->params.enableNoise != 0);
            if (ImGui::Checkbox("ノイズを有効化", &enableNoise)) {
                currentItem->params.enableNoise = enableNoise ? 1 : 0;
            }
            if (enableNoise) {
                ImGui::Spacing();
                DrawFloatControl("ノイズ強度", &currentItem->params.noiseStrength, 0.0f, 1.0f, 0.005f);
                ImGui::Spacing();
                DrawFloatControl("ノイズスケール", &currentItem->params.noiseScale, 1.0f, 1000.0f, 1.0f);
                ImGui::Spacing();

                ImGui::Text("ノイズ合成モード (Blend Mode)");
                const char *blendModeNames[] = {
                    "通常 (Normal)",
                    "加算 (Add)",
                    "乗算 (Multiply)",
                    "スクリーン (Screen)",
                    "オーバーレイ (Overlay)"
                };
                ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
                ImGui::Combo("##NoiseBlendMode", &currentItem->params.noiseBlendMode, blendModeNames, IM_ARRAYSIZE(blendModeNames));
                ImGui::PopItemWidth();
            }
            ImGui::Spacing();
        }
        ImGui::Spacing();
    }

    // --- アイリス ---
    if (currentItem->shaderType == PostEffectShaderType::Iris || isAll) {
        if (ImGui::CollapsingHeader("アイリス設定 (Iris)", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Spacing();
            bool enableIris = (currentItem->params.enableIris != 0);
            if (ImGui::Checkbox("アイリスを有効化", &enableIris)) {
                currentItem->params.enableIris = enableIris ? 1 : 0;
            }
            if (enableIris) {
                ImGui::Spacing();
                const char* irisModes[] = {
                    "アイリスアウト (Iris Out / 閉じる)",
                    "アイリスイン (Iris In / 開く)"
                };
                int currentMode = currentItem->params.isIrisIn;
                ImGui::Text("アイリスモード (Iris Mode)");
                ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
                if (ImGui::Combo("##IrisMode", &currentMode, irisModes, IM_ARRAYSIZE(irisModes))) {
                    currentItem->params.isIrisIn = currentMode;
                }
                ImGui::PopItemWidth();
                ImGui::Spacing();

                DrawFloatControl("基準中心 X (Center X)", &currentItem->params.irisCenter[0], 0.0f, 1.0f, 0.005f);
                ImGui::Spacing();
                DrawFloatControl("基準中心 Y (Center Y)", &currentItem->params.irisCenter[1], 0.0f, 1.0f, 0.005f);
                ImGui::Spacing();
                DrawFloatControl("半径 (Radius)", &currentItem->params.irisRadius, 0.0f, 2.0f, 0.005f);
                ImGui::Spacing();
                DrawFloatControl("滑らかさ (Smoothness)", &currentItem->params.irisSmoothness, 0.0f, 0.5f, 0.001f);
                ImGui::Spacing();

                ImGui::Text("マスクカラー (Mask Color)");
                ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
                ImGui::ColorEdit4("##irisMaskColor", currentItem->params.irisMaskColor);
                ImGui::PopItemWidth();
            }
            ImGui::Spacing();
        }
        ImGui::Spacing();
    }
}

#endif
