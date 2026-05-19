#ifdef USE_IMGUI
#include "EditorManager.h"
#include "Core/Utility/TransformFunctions.h"
#include "Input/KeyboardInput.h"
#include "Renderer/SrvManager.h"
#include "Scene/SceneManager.h"
#include "Scene/IScene.h"
#include "GameObject/Object3D.h"
#include "GameObject/PrimitiveObject.h"
#include "Effect/ParticleManager.h"
#include "Renderer/DirectXCommon/DirectXCommon.h"

// ImGuiのヘッダー (パスは環境に合わせてください)
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_win32.h>

#include <cmath>
#include <numbers>
#include <fstream>
#include <string>
#include <filesystem>

// 枠を借りるための関数 (WindowsApplication.cppからお引越し)
static void ImGuiSrvAlloc(ImGui_ImplDX12_InitInfo *info, D3D12_CPU_DESCRIPTOR_HANDLE *out_cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE *out_gpu_handle) {
    SrvManager::GetInstance()->Allocate(out_cpu_handle, out_gpu_handle);
}

bool EditorManager::showObjects_ = true;
bool EditorManager::showEffects_ = true;

// 枠を返すための関数
static void ImGuiSrvFree(ImGui_ImplDX12_InitInfo *info, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle) {
    // 空でOK
}

void EditorManager::Initialize(HWND hwnd, ID3D12Device *device, ID3D12CommandQueue *commandQueue) {
    // 1. ImGuiコンテキストの作成
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    // 2. フラグの設定
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;   // ドッキング有効化

    // 3. Win32バックエンドの初期化
    ImGui_ImplWin32_Init(hwnd);

    // 4. DirectX12バックエンドの初期化
    ImGui_ImplDX12_InitInfo init_info = {};
    init_info.Device = device;
    init_info.CommandQueue = commandQueue;
    // BufferCount は決め打ち(通常2か3)にするか、引数で貰うかします（ここでは一般的な2とします）
    init_info.NumFramesInFlight = 2;
    init_info.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    init_info.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    init_info.SrvDescriptorHeap = SrvManager::GetInstance()->GetSrvDescriptorHeap();
    init_info.SrvDescriptorAllocFn = ImGuiSrvAlloc;
    init_info.SrvDescriptorFreeFn = ImGuiSrvFree;

    ImGui_ImplDX12_Init(&init_info);

    // 5. スタイルの微調整
    ImGuiStyle &style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }
}

void EditorManager::BeginFrame() {
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void EditorManager::UpdateUI(ModelCommon *modelCommon, GameCamera *gameCamera, DebugCamera *debugCamera, Camera **activeCamera, bool &isDebugCameraActive, D3D12_GPU_DESCRIPTOR_HANDLE renderTextureSrvHandle, SceneManager *sceneManager) {

    // --- メインメニューバー ---
    if (ImGui::BeginMainMenuBar()) {
        // PLAY / STOP ボタン
        if (!isPlaying_) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.5f, 0.0f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.0f, 0.7f, 0.0f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.0f, 0.4f, 0.0f, 1.0f));
            if (ImGui::Button("PLAY")) {
                isPlaying_ = true;
                useDebugCamera_ = false;
            }
            ImGui::PopStyleColor(3);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.0f, 0.0f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.0f, 0.0f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.0f, 0.0f, 1.0f));
            if (ImGui::Button("STOP")) {
                isPlaying_ = false;
                useDebugCamera_ = true;
                debugCamera->SetTranslation(gameCamera->GetTranslation());
                debugCamera->SetRotation(gameCamera->GetRotation());
            }
            ImGui::PopStyleColor(3);
        }

        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);

        // Debug Camera チェックボックス
        ImGui::Checkbox("Debug Camera", &useDebugCamera_);

        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);

        // シーン切替コンボ
        ImGui::Text("Scene:");
        ImGui::SetNextItemWidth(130.0f);
        {
            int sceneIndex = static_cast<int>(currentSceneType_);
            if (ImGui::Combo("##Scene", &sceneIndex, kSceneTypeNames, static_cast<int>(SceneType::kCount))) {
                SceneType newType = static_cast<SceneType>(sceneIndex);
                if (newType != currentSceneType_) {
                    currentSceneType_ = newType;
                    sceneManager->ChangeScene(SceneFactory::CreateScene(currentSceneType_));
                    selectedObject_ = nullptr;
                    selectedParticle_ = nullptr;
                    selectedPrimitive_ = nullptr;
                    SaveSceneConfig();
                }
            }
        }

        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);

        // Window開閉制御メニュー
        if (ImGui::BeginMenu("Window")) {
            ImGui::MenuItem("Inspector", nullptr, &showInspector_);
            ImGui::MenuItem("Hierarchy", nullptr, &showHierarchy_);
            ImGui::MenuItem("Game View", nullptr, &showGameView_);
            ImGui::MenuItem("PostEffect", nullptr, &showPostEffect_);
            ImGui::EndMenu();
        }

        // インスペクターが閉じている場合に表示する復元ボタン
        if (!showInspector_) {
            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.45f, 0.8f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.55f, 0.9f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.05f, 0.35f, 0.7f, 1.0f));
            if (ImGui::Button("Open Inspector")) {
                showInspector_ = true;
            }
            ImGui::PopStyleColor(3);
        }

        ImGui::EndMainMenuBar();
    }

    // --- ドッキングの設定 ---
    ImGuiID dockspace_id = ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

    // 初回起動時にレイアウトを自動的に構成する
    static bool first_time = true;
    if (first_time) {
        first_time = false;

        // 一度ノードをクリアして再構築
        ImGui::DockBuilderRemoveNode(dockspace_id);
        ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->Size);

        ImGuiID dock_id_main = dockspace_id;
        // 左側に「Hierarchy」
        ImGuiID dock_id_left = ImGui::DockBuilderSplitNode(dock_id_main, ImGuiDir_Left, 0.20f, NULL, &dock_id_main);
        // 右側に「Inspector」
        ImGuiID dock_id_right = ImGui::DockBuilderSplitNode(dock_id_main, ImGuiDir_Right, 0.25f, NULL, &dock_id_main);

        // 各ウィンドウを各ノードに割り当てる
        ImGui::DockBuilderDockWindow("Game View", dock_id_main);
        ImGui::DockBuilderDockWindow("Hierarchy", dock_id_left);
        ImGui::DockBuilderDockWindow("Inspector", dock_id_right);
        ImGui::DockBuilderDockWindow("PostEffect", dock_id_right);

        ImGui::DockBuilderFinish(dockspace_id);
    }

    // --- Game View ウィンドウ ---
    if (showGameView_) {
        if (ImGui::Begin("Game View", &showGameView_)) {
            isGameViewHovered_ = ImGui::IsWindowHovered();
            {
                // ウィンドウのサイズに合わせてアスペクト比を維持して描画
                ImVec2 contentSize = ImGui::GetContentRegionAvail();
                float aspect = 1280.0f / 720.0f; // 元のアスペクト比
                float windowAspect = contentSize.x / contentSize.y;
                ImVec2 imageSize;
                if (windowAspect > aspect) {
                    imageSize.y = contentSize.y;
                    imageSize.x = contentSize.y * aspect;
                } else {
                    imageSize.x = contentSize.x;
                    imageSize.y = contentSize.x / aspect;
                }
                // 中央寄せ
                ImVec2 currentPos = ImGui::GetCursorPos();
                ImGui::SetCursorPos(ImVec2(currentPos.x + (contentSize.x - imageSize.x) * 0.5f, currentPos.y + (contentSize.y - imageSize.y) * 0.5f));
                ImGui::Image((ImTextureID)renderTextureSrvHandle.ptr, imageSize);
            }
        }
        ImGui::End();
    }

    // --- Hierarchy ウィンドウ ---
    if (showHierarchy_) {
        if (ImGui::Begin("Hierarchy", &showHierarchy_)) {
            IScene *activeScene = sceneManager->GetCurrentScene();
            if (activeScene) {
                if (ImGui::CollapsingHeader("Objects", ImGuiTreeNodeFlags_DefaultOpen)) {
                    for (auto *obj : activeScene->GetObjects()) {
                        bool isSelected = (selectedObject_ == obj);
                        if (ImGui::Selectable(obj->GetName().c_str(), isSelected)) {
                            selectedObject_ = obj;
                            selectedParticle_ = nullptr;
                            selectedPrimitive_ = nullptr;
                        }
                    }
                }
                if (ImGui::CollapsingHeader("Particles", ImGuiTreeNodeFlags_DefaultOpen)) {
                    for (auto *particle : activeScene->GetParticles()) {
                        bool isSelected = (selectedParticle_ == particle);
                        if (ImGui::Selectable(particle->GetName().c_str(), isSelected)) {
                            selectedParticle_ = particle;
                            selectedObject_ = nullptr;
                            selectedPrimitive_ = nullptr;
                        }
                    }
                }
                if (ImGui::CollapsingHeader("Primitives", ImGuiTreeNodeFlags_DefaultOpen)) {
                    for (auto *primitive : activeScene->GetPrimitives()) {
                        bool isSelected = (selectedPrimitive_ == primitive);
                        if (ImGui::Selectable(primitive->GetName().c_str(), isSelected)) {
                            selectedPrimitive_ = primitive;
                            selectedObject_ = nullptr;
                            selectedParticle_ = nullptr;
                        }
                    }
                }
            }
        }
        ImGui::End();
    }

    // --- デバッグカメラの切り替え制御 ---
    if (KeyboardInput::GetInstance()->IsKeyPressed(DIK_F3)) {
        useDebugCamera_ = !useDebugCamera_;
        if (useDebugCamera_) {
            debugCamera->SetTranslation(gameCamera->GetTranslation());
            debugCamera->SetRotation(gameCamera->GetRotation());
        }
    }

    // --- Inspector ウィンドウ ---
    if (showInspector_) {
        if (ImGui::Begin("Inspector", &showInspector_)) {
            if (selectedObject_ || selectedParticle_ || selectedPrimitive_) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.25f, 0.3f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.35f, 0.45f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.2f, 0.25f, 1.0f));
                if (ImGui::Button("Show Global Settings", ImVec2(-1, 0))) {
                    selectedObject_ = nullptr;
                    selectedParticle_ = nullptr;
                    selectedPrimitive_ = nullptr;
                }
                ImGui::PopStyleColor(3);
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();
            }

            if (selectedObject_) {
                selectedObject_->DisplayImGui("Object Properties");
            } else if (selectedParticle_) {
                selectedParticle_->DrawImGui();
            } else if (selectedPrimitive_) {
                selectedPrimitive_->DisplayImGui("Primitive Properties");
            } else {
                ImGui::Text("Global Visibility Settings");
                ImGui::Separator();
                ImGui::Checkbox("Show Objects (Models)", &showObjects_);
                ImGui::Checkbox("Show Effects (Particles/Primitives)", &showEffects_);
                ImGui::Spacing();

                ImGui::Text("Outline Settings");
                ImGui::Separator();
                {
                    auto dxCommon = DirectXCommon::GetInstance();
                    bool outlineEnabled = dxCommon->IsOutlineEnabled();
                    if (ImGui::Checkbox("Enable Outline", &outlineEnabled)) {
                        dxCommon->SetOutlineEnabled(outlineEnabled);
                        SaveSceneConfig();
                    }
                }
                ImGui::Spacing();

                ImGui::Text("Global Settings (Lighting)");
                ImGui::Separator();

                static int activeLightType = 2;
                static bool enableFog = false;
                static float dIntensity = 1.0f;
                static float pIntensity = 1.0f;
                static float sIntensity = 4.0f;
                static float spotAngleDeg = 30.0f;
                static float spotFalloffDeg = 20.0f;

                ImGui::Text("Active Light Source");
                ImGui::RadioButton("Directional", &activeLightType, 0);
                ImGui::SameLine();
                ImGui::RadioButton("Point", &activeLightType, 1);
                ImGui::SameLine();
                ImGui::RadioButton("Spot", &activeLightType, 2);
                ImGui::Separator();

                DirectionalLight *dLight = modelCommon->GetDirectionalLight();
                PointLight *pLight = modelCommon->GetPointLight();
                SpotLight *sLight = modelCommon->GetSpotLight();

                if (activeLightType == 0) {
                    dLight->intensity = dIntensity;
                    pLight->intensity = 0.0f;
                    sLight->intensity = 0.0f;
                    ImGui::Text("Directional Light Settings");
                    ImGui::ColorEdit4("Color", &dLight->color.x);
                    ImGui::DragFloat("Intensity", &dIntensity, 0.01f, 0.0f, 10.0f);
                    ImGui::DragFloat3("Direction", &dLight->direction.x, 0.01f, -1.0f, 1.0f);
                    dLight->direction = TransformFunctions::Normalize(dLight->direction);
                } else if (activeLightType == 1) {
                    pLight->intensity = pIntensity;
                    dLight->intensity = 0.0f;
                    sLight->intensity = 0.0f;
                    ImGui::Text("Point Light Settings");
                    ImGui::ColorEdit4("Color", &pLight->color.x);
                    ImGui::DragFloat("Intensity", &pIntensity, 0.01f, 0.0f, 10.0f);
                    ImGui::DragFloat3("Position", &pLight->position.x, 0.1f);
                    ImGui::DragFloat("Radius", &pLight->radius, 0.1f, 0.0f, 100.0f);
                    ImGui::DragFloat("Decay", &pLight->decay, 0.01f, 0.0f, 10.0f);
                } else if (activeLightType == 2) {
                    sLight->intensity = sIntensity;
                    dLight->intensity = 0.0f;
                    pLight->intensity = 0.0f;
                    ImGui::Text("Spot Light Settings");
                    ImGui::ColorEdit4("Color", &sLight->color.x);
                    ImGui::DragFloat("Intensity", &sIntensity, 0.01f, 0.0f, 20.0f);
                    ImGui::DragFloat3("Position", &sLight->position.x, 0.1f);
                    if (ImGui::DragFloat3("Direction", &sLight->direction.x, 0.01f, -1.0f, 1.0f)) {
                        sLight->direction = TransformFunctions::Normalize(sLight->direction);
                    }
                    ImGui::DragFloat("Distance", &sLight->distance, 0.1f, 0.0f, 100.0f);
                    ImGui::DragFloat("Decay", &sLight->decay, 0.01f, 0.0f, 10.0f);
                    ImGui::SliderFloat("Total Angle", &spotAngleDeg, 0.0f, 90.0f);
                    ImGui::SliderFloat("Falloff Start", &spotFalloffDeg, 0.0f, spotAngleDeg);
                    sLight->cosAngle = std::cos(spotAngleDeg * (std::numbers::pi_v<float> / 180.0f));
                    sLight->cosFalloffStart = std::cos(spotFalloffDeg * (std::numbers::pi_v<float> / 180.0f));
                }

                ImGui::Separator();
                ImGui::Checkbox("Enable Fog Effect", &enableFog);
            }
        }
        ImGui::End();
    }

    // --- PostEffect ウィンドウ ---
    if (showPostEffect_) {
        if (ImGui::Begin("PostEffect", &showPostEffect_)) {
            ImGui::Text("Post Effect Settings");
            ImGui::Separator();
            const char* postEffectItems[] = { "None", "Grayscale", "Sepia", "Vignette", "Smoothing" };
            int currentEffect = (int)DirectXCommon::GetInstance()->GetPostEffect();
            if (ImGui::Combo("Effect Type", &currentEffect, postEffectItems, IM_ARRAYSIZE(postEffectItems))) {
                DirectXCommon::GetInstance()->SetPostEffect((DirectXCommon::PostEffect)currentEffect);
            }

            if (currentEffect == (int)DirectXCommon::PostEffect::kVignette) {
                ImGui::Spacing();
                ImGui::Text("Vignette Settings");
                ImGui::Separator();
                auto params = DirectXCommon::GetInstance()->GetVignetteParamsData();
                if (params) {
                    ImGui::ColorEdit4("Color", params->color);
                    ImGui::DragFloat("Scale", &params->scale, 0.1f, 0.0f, 100.0f);
                    ImGui::DragFloat("Power", &params->power, 0.01f, 0.0f, 10.0f);
                }
            }

            if (currentEffect == (int)DirectXCommon::PostEffect::kSmoothing) {
                ImGui::Spacing();
                ImGui::Text("Smoothing Settings");
                ImGui::Separator();
                auto params = DirectXCommon::GetInstance()->GetSmoothingParamsData();
                if (params) {
                    ImGui::SliderInt("Kernel Size", &params->kernelSize, 1, 5);
                    ImGui::SliderFloat("Strength", &params->strength, 0.0f, 1.0f);
                }
            }
        }
        ImGui::End();
    }
}

void EditorManager::Draw(ID3D12GraphicsCommandList *commandList) {
    ImGui::Render();
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
}

void EditorManager::Finalize() {
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

void EditorManager::SaveSceneConfig() {
    // ディレクトリが存在しない場合は作成する
    std::filesystem::create_directories("json");

    std::ofstream ofs("json/editor_config.json");
    if (ofs.is_open()) {
        auto dxCommon = DirectXCommon::GetInstance();
        ofs << "{" << std::endl;
        ofs << "  \"currentScene\": \"" << SceneFactory::GetSceneTypeName(currentSceneType_) << "\"," << std::endl;
        ofs << "  \"isOutlineEnabled\": " << (dxCommon->IsOutlineEnabled() ? "true" : "false") << "," << std::endl;
        ofs << "  \"outlineThickness\": " << dxCommon->GetOutlineThickness() << std::endl;
        ofs << "}" << std::endl;
        ofs.close();
    }
}

void EditorManager::LoadSceneConfig() {
    std::ifstream ifs("json/editor_config.json");
    if (!ifs.is_open()) {
        return; // ファイルがない場合はデフォルトのまま
    }

    std::string content;
    std::string line;
    while (std::getline(ifs, line)) {
        content += line;
    }
    ifs.close();

    // 簡易的なJSONパーサー: "currentScene": "xxx" を抽出
    {
        const std::string key = "\"currentScene\"";
        size_t keyPos = content.find(key);
        if (keyPos != std::string::npos) {
            // ':' の後の最初の '"' を見つける
            size_t colonPos = content.find(':', keyPos + key.length());
            if (colonPos != std::string::npos) {
                size_t valueStart = content.find('"', colonPos + 1);
                if (valueStart != std::string::npos) {
                    valueStart++; // '"' の次の文字から
                    size_t valueEnd = content.find('"', valueStart);
                    if (valueEnd != std::string::npos) {
                        std::string sceneName = content.substr(valueStart, valueEnd - valueStart);
                        currentSceneType_ = SceneFactory::GetSceneTypeFromName(sceneName);
                    }
                }
            }
        }
    }

    // isOutlineEnabled のパース
    {
        const std::string key = "\"isOutlineEnabled\"";
        size_t keyPos = content.find(key);
        if (keyPos != std::string::npos) {
            size_t colonPos = content.find(':', keyPos + key.length());
            if (colonPos != std::string::npos) {
                size_t valPos = content.find_first_not_of(" \t\r\n", colonPos + 1);
                if (valPos != std::string::npos) {
                    if (content.compare(valPos, 4, "true") == 0) {
                        DirectXCommon::GetInstance()->SetOutlineEnabled(true);
                    } else if (content.compare(valPos, 5, "false") == 0) {
                        DirectXCommon::GetInstance()->SetOutlineEnabled(false);
                    }
                }
            }
        }
    }

    // outlineThickness のパース
    {
        const std::string key = "\"outlineThickness\"";
        size_t keyPos = content.find(key);
        if (keyPos != std::string::npos) {
            size_t colonPos = content.find(':', keyPos + key.length());
            if (colonPos != std::string::npos) {
                size_t valPos = content.find_first_not_of(" \t\r\n", colonPos + 1);
                if (valPos != std::string::npos) {
                    size_t endPos = content.find_first_of(",}", valPos);
                    if (endPos != std::string::npos) {
                        std::string numStr = content.substr(valPos, endPos - valPos);
                        try {
                            float thickness = std::stof(numStr);
                            DirectXCommon::GetInstance()->SetOutlineThickness(thickness);
                        } catch (...) {
                            // 例外は無視
                        }
                    }
                }
            }
        }
    }
}
#endif