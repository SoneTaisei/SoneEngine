#include "PlayerChainPostEffect.h"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <nlohmann/json.hpp>

void PlayerChainPostEffect::Initialize(const std::string& jsonPath) {
    filePath_ = jsonPath;
    LoadFromJson(filePath_);
}

bool PlayerChainPostEffect::LoadFromJson(const std::string& jsonPath) {
    filePath_ = jsonPath;
    if (!std::filesystem::exists(filePath_)) {
        return false;
    }

    try {
        std::ifstream file(filePath_);
        if (!file.is_open()) return false;

        nlohmann::json root;
        file >> root;

        postEffects_.clear();
        if (root.contains("postEffects") && root["postEffects"].is_array()) {
            for (const auto& j : root["postEffects"]) {
                PostEffectItem item;
                item.name = j.value("name", "PostEffect");
                item.enabled = j.value("enabled", true);
                int rawType = j.value("shaderType", static_cast<int>(PostEffectShaderType::Sepia));
                if (rawType < 0 || rawType >= static_cast<int>(PostEffectShaderType::Count)) {
                    item.shaderType = PostEffectShaderType::Letterbox;
                } else {
                    item.shaderType = static_cast<PostEffectShaderType>(rawType);
                }

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
                    if (s.contains("letterbox")) {
                        item.params.enableLetterbox = s["letterbox"].value("enabled", true) ? 1 : 0;
                        item.params.letterboxHeight = s["letterbox"].value("height", 0.12f);
                        item.params.letterboxSmoothness = s["letterbox"].value("smoothness", 0.002f);
                        if (s["letterbox"].contains("color") && s["letterbox"]["color"].is_array()) {
                            for (size_t c = 0; c < 4 && c < s["letterbox"]["color"].size(); ++c) {
                                item.params.letterboxColor[c] = s["letterbox"]["color"][c].get<float>();
                            }
                        }
                    }
                }
                postEffects_.push_back(item);
            }
        }

        lastFileWriteTime_ = std::filesystem::last_write_time(filePath_);
        return true;
    } catch (...) {
        return false;
    }
}

void PlayerChainPostEffect::Update(float dt, bool isTriggered) {
    // ホットリロード監視（ファイル更新日時が変わっていたら自動再読み込み）
    try {
        if (!filePath_.empty() && std::filesystem::exists(filePath_)) {
            auto currentWriteTime = std::filesystem::last_write_time(filePath_);
            if (currentWriteTime != lastFileWriteTime_) {
                LoadFromJson(filePath_);
            }
        }
    } catch (...) {}

    // アルファ値のスムーズな補間
    if (isTriggered) {
        if (fadeInDuration_ > 0.0f) {
            currentAlpha_ += dt / fadeInDuration_;
        } else {
            currentAlpha_ = 1.0f;
        }
        currentAlpha_ = (std::min)(1.0f, currentAlpha_);
    } else {
        if (fadeOutDuration_ > 0.0f) {
            currentAlpha_ -= dt / fadeOutDuration_;
        } else {
            currentAlpha_ = 0.0f;
        }
        currentAlpha_ = (std::max)(0.0f, currentAlpha_);
    }
}

void PlayerChainPostEffect::ApplyToDirectXCommon(DirectXCommon* dxCommon) {
    if (!dxCommon) return;

    auto target = dxCommon->GetCompositeParamsData();
    if (!target) return;

    if (currentAlpha_ > 0.001f) {
        wasApplied_ = true;
        dxCommon->SetPostEffectEnabled(true);
        dxCommon->SetPostEffect(DirectXCommon::PostEffect::kComposite);

        for (const auto& item : postEffects_) {
            if (!item.enabled) continue;

            bool isAll = (item.shaderType == PostEffectShaderType::Composite);

            // Letterbox
            if (item.shaderType == PostEffectShaderType::Letterbox || isAll) {
                if (item.params.enableLetterbox) {
                    target->enableLetterbox = 1;
                    target->letterboxHeight = (std::max)(target->letterboxHeight, item.params.letterboxHeight * currentAlpha_);
                    target->letterboxSmoothness = item.params.letterboxSmoothness;
                    for (int i = 0; i < 3; ++i) {
                        target->letterboxColor[i] = item.params.letterboxColor[i];
                    }
                    target->letterboxColor[3] = item.params.letterboxColor[3] * currentAlpha_;
                }
            }

            // Vignette
            if (item.shaderType == PostEffectShaderType::Vignette || isAll) {
                if (item.params.enableVignette) {
                    target->enableVignette = 1;
                    target->vignetteScale = item.params.vignetteScale;
                    target->vignettePower = (std::max)(target->vignettePower, item.params.vignettePower * currentAlpha_);
                    for (int i = 0; i < 4; ++i) {
                        target->vignetteColor[i] = item.params.vignetteColor[i];
                    }
                }
            }

            // Blur
            if (item.shaderType == PostEffectShaderType::Blur || isAll) {
                if (item.params.blurType != 0) {
                    target->blurType = item.params.blurType;
                    target->boxBlurKernelSize = item.params.boxBlurKernelSize;
                    target->boxBlurStrength = item.params.boxBlurStrength * currentAlpha_;
                    target->gaussianSigma = item.params.gaussianSigma * currentAlpha_;
                }
            }

            // Radial Blur
            if (item.shaderType == PostEffectShaderType::RadialBlur || isAll) {
                if (item.params.enableRadialBlur) {
                    target->enableRadialBlur = 1;
                    target->radialBlurCenter[0] = item.params.radialBlurCenter[0];
                    target->radialBlurCenter[1] = item.params.radialBlurCenter[1];
                    target->radialBlurWidth = (std::max)(target->radialBlurWidth, item.params.radialBlurWidth * currentAlpha_);
                    target->radialBlurSamples = item.params.radialBlurSamples;
                }
            }

            // Grayscale
            if (item.shaderType == PostEffectShaderType::Grayscale || isAll) {
                if (item.params.grayscaleStrength > 0.0f) {
                    target->grayscaleStrength = (std::max)(target->grayscaleStrength, item.params.grayscaleStrength * currentAlpha_);
                }
            }

            // Sepia
            if (item.shaderType == PostEffectShaderType::Sepia || isAll) {
                if (item.params.sepiaStrength > 0.0f) {
                    target->sepiaStrength = (std::max)(target->sepiaStrength, item.params.sepiaStrength * currentAlpha_);
                }
            }

            // Noise
            if (item.shaderType == PostEffectShaderType::Noise || isAll) {
                if (item.params.enableNoise) {
                    target->enableNoise = 1;
                    target->noiseStrength = item.params.noiseStrength * currentAlpha_;
                    target->noiseScale = item.params.noiseScale;
                    target->noiseBlendMode = item.params.noiseBlendMode;
                }
            }

            // Dissolve
            if (item.shaderType == PostEffectShaderType::Dissolve || isAll) {
                if (item.params.enableDissolve) {
                    target->enableDissolve = 1;
                    target->dissolveThreshold = item.params.dissolveThreshold * currentAlpha_;
                    target->dissolveEdgeWidth = item.params.dissolveEdgeWidth;
                    for (int i = 0; i < 3; ++i) {
                        target->dissolveEdgeColor[i] = item.params.dissolveEdgeColor[i];
                        target->dissolveBgColor[i] = item.params.dissolveBgColor[i];
                    }
                }
            }

            // Iris
            if (item.shaderType == PostEffectShaderType::Iris || isAll) {
                if (item.params.enableIris) {
                    target->enableIris = 1;
                    target->irisCenter[0] = item.params.irisCenter[0];
                    target->irisCenter[1] = item.params.irisCenter[1];
                    target->irisRadius = item.params.irisRadius;
                    target->irisSmoothness = item.params.irisSmoothness;
                    target->isIrisIn = item.params.isIrisIn;
                    for (int i = 0; i < 4; ++i) {
                        target->irisMaskColor[i] = item.params.irisMaskColor[i];
                    }
                }
            }
        }
    } else if (wasApplied_) {
        // 完全オフになったフレームで確実にリセット
        Reset(dxCommon);
    }
}

void PlayerChainPostEffect::Reset(DirectXCommon* dxCommon) {
    currentAlpha_ = 0.0f;
    if (wasApplied_ && dxCommon) {
        auto target = dxCommon->GetCompositeParamsData();
        if (target) {
            target->enableLetterbox = 0;
            target->letterboxHeight = 0.0f;
            target->enableVignette = 0;
            target->blurType = 0;
            target->enableRadialBlur = 0;
            target->enableNoise = 0;
            target->grayscaleStrength = 0.0f;
            target->sepiaStrength = 0.0f;
            target->enableDissolve = 0;
        }
    }
    wasApplied_ = false;
}

#ifdef USE_IMGUI
void PlayerChainPostEffect::DisplayImGui() {
    if (ImGui::CollapsingHeader("Player Chain PostEffect")) {
        ImGui::Text("File: %s", filePath_.c_str());
        ImGui::ProgressBar(currentAlpha_, ImVec2(0.0f, 0.0f), "Effect Alpha");
        ImGui::DragFloat("Fade In Duration (s)", &fadeInDuration_, 0.01f, 0.0f, 1.0f, "%.2f");
        ImGui::DragFloat("Fade Out Duration (s)", &fadeOutDuration_, 0.01f, 0.0f, 1.0f, "%.2f");
        if (ImGui::Button("Reload Player_Chain.json")) {
            LoadFromJson(filePath_);
        }
    }
}
#endif
