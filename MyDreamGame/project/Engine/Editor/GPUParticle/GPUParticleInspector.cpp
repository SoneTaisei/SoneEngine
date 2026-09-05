#ifdef USE_IMGUI
#include "GPUParticleInspector.h"
#include "GPUParticleEditorContext.h"
#include "Graphics/TextureManager.h"
#include <algorithm>

void GPUParticleInspector::Initialize() {}

void GPUParticleInspector::DrawInspectorUI(SceneManager* /*sceneManager*/, GPUParticleEditorContext* context) {
    if (!context) return;

    auto emitter = context->GetSelectedEmitter();
    if (!emitter) {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "エミッターが選択されていません。");
        ImGui::Text("左側のエミッター階層からエミッターを選択するか、\n[+ エミッター追加] をクリックしてください。");
        return;
    }

    auto& data = emitter->GetData();

    // ヘッダー（エミッター名 & Undo/Redo）
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.75f, 0.30f, 1.0f));
    ImGui::Text("エミッター: %s", data.name.c_str());
    ImGui::PopStyleColor();

    ImGui::SameLine();
    if (!context->CanUndo()) ImGui::BeginDisabled();
    if (ImGui::Button("戻る##Undo")) context->PerformUndo();
    if (!context->CanUndo()) ImGui::EndDisabled();

    ImGui::SameLine();
    if (!context->CanRedo()) ImGui::BeginDisabled();
    if (ImGui::Button("進む##Redo")) context->PerformRedo();
    if (!context->CanRedo()) ImGui::EndDisabled();

    ImGui::Separator();

    // エミッター名変更
    char nameBuf[128];
    strncpy_s(nameBuf, data.name.c_str(), sizeof(nameBuf) - 1);
    if (ImGui::InputText("エミッター名", nameBuf, sizeof(nameBuf))) {
        data.name = nameBuf;
    }
    if (ImGui::IsItemActivated()) context->PushUndoState("Rename Emitter");

    // 1. レンダラー設定 (最重要)
    DrawRendererSection(context);

    // 2. 発生 & 寿命設定
    DrawSpawnSection(context);

    // 3. 発生形状設定
    DrawShapeSection(context);

    // 4. 物理 & 運動設定
    DrawPhysicsSection(context);

    // 5. サイズ & 回転設定
    DrawTransformSection(context);

    // 6. カラー & アルファ設定
    DrawColorSection(context);
}

void GPUParticleInspector::DrawRendererSection(GPUParticleEditorContext* context) {
    auto emitter = context->GetSelectedEmitter();
    if (!emitter) return;
    auto& data = emitter->GetData();

    if (ImGui::CollapsingHeader("レンダラー設定 (Renderer)", ImGuiTreeNodeFlags_DefaultOpen)) {
        // 描画タイプ (スプライト / 3Dメッシュ)
        int renderTypeIdx = static_cast<int>(data.renderType);
        const char* renderTypeNames[] = { "板ポリゴン (Sprite / Quad)", "3Dモデルメッシュ (Mesh)" };
        if (ImGui::Combo("描画タイプ", &renderTypeIdx, renderTypeNames, IM_ARRAYSIZE(renderTypeNames))) {
            context->PushUndoState("Change Render Type");
            data.renderType = static_cast<GPUParticleRenderType>(renderTypeIdx);
        }

        // 3Dモデル選択 (Mesh時のみ)
        if (data.renderType == GPUParticleRenderType::Mesh) {
            const auto& models = context->GetAvailableModels();
            int currentModelIdx = -1;
            for (size_t i = 0; i < models.size(); ++i) {
                if (models[i] == data.modelPath) {
                    currentModelIdx = static_cast<int>(i);
                    break;
                }
            }
            std::string previewName = currentModelIdx >= 0 ? models[currentModelIdx] : data.modelPath;
            if (ImGui::BeginCombo("3Dモデル", previewName.c_str())) {
                for (size_t i = 0; i < models.size(); ++i) {
                    bool isSelected = (currentModelIdx == static_cast<int>(i));
                    if (ImGui::Selectable(models[i].c_str(), isSelected)) {
                        context->PushUndoState("Change Model");
                        data.modelPath = models[i];
                    }
                    if (isSelected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("プロジェクト内のOBJ/GLTFモデル");
        }

        // テクスチャ選択
        const auto& textures = context->GetAvailableTextures();
        int currentTexIdx = -1;
        for (size_t i = 0; i < textures.size(); ++i) {
            if (textures[i] == data.texturePath) {
                currentTexIdx = static_cast<int>(i);
                break;
            }
        }
        std::string previewTexName = currentTexIdx >= 0 ? textures[currentTexIdx] : data.texturePath;
        if (ImGui::BeginCombo("テクスチャ", previewTexName.c_str())) {
            for (size_t i = 0; i < textures.size(); ++i) {
                bool isSelected = (currentTexIdx == static_cast<int>(i));
                if (ImGui::Selectable(textures[i].c_str(), isSelected)) {
                    context->PushUndoState("Change Texture");
                    data.texturePath = textures[i];
                }
                if (isSelected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        // テクスチャプレビュー
        uint32_t texHandle = TextureManager::GetInstance()->Load(data.texturePath);
        D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = TextureManager::GetInstance()->GetGpuHandle(texHandle);
        if (gpuHandle.ptr != 0) {
            ImGui::Image((ImTextureID)gpuHandle.ptr, ImVec2(48, 48), ImVec2(0, 0), ImVec2(1, 1), ImVec4(1, 1, 1, 1), ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
            ImGui::SameLine();
            ImGui::Text("テクスチャプレビュー\nパス: %s", data.texturePath.c_str());
        }

        // ビルボード方式
        int billboardIdx = static_cast<int>(data.billboardType);
        const char* billboardNames[] = {
            "なし (None / 3D回転そのまま)",
            "全方向 (All-Axis / カメラ正面)",
            "Y軸固定 (Y-Axis / 水平のみ追従)",
            "進行方向ストレッチ (Velocity Stretch)"
        };
        if (ImGui::Combo("ビルボード方式", &billboardIdx, billboardNames, IM_ARRAYSIZE(billboardNames))) {
            context->PushUndoState("Change Billboard Type");
            data.billboardType = static_cast<GPUParticleBillboardType>(billboardIdx);
        }

        if (data.billboardType == GPUParticleBillboardType::VelocityStretch) {
            if (ImGui::DragFloat("ストレッチ倍率", &data.stretchFactor, 0.01f, 0.0f, 2.0f, "%.2f")) {
                // 更新
            }
            if (ImGui::IsItemActivated()) context->PushUndoState("Change Stretch Factor");
        }

        // ブレンドモード
        int blendIdx = static_cast<int>(data.blendMode);
        const char* blendNames[] = { "なし (None)", "通常α (Normal)", "加算 (Add)", "減算 (Subtract)", "乗算 (Multiply)", "スクリーン (Screen)" };
        if (blendIdx >= 0 && blendIdx < IM_ARRAYSIZE(blendNames)) {
            if (ImGui::Combo("合成モード", &blendIdx, blendNames, IM_ARRAYSIZE(blendNames))) {
                context->PushUndoState("Change Blend Mode");
                data.blendMode = static_cast<BlendMode>(blendIdx);
            }
        }
    }
}

void GPUParticleInspector::DrawSpawnSection(GPUParticleEditorContext* context) {
    auto emitter = context->GetSelectedEmitter();
    if (!emitter) return;
    auto& data = emitter->GetData();

    if (ImGui::CollapsingHeader("発生 & 寿命 (Spawn & Lifetime)", ImGuiTreeNodeFlags_DefaultOpen)) {
        // 最大粒子数
        int maxP = static_cast<int>(data.maxParticles);
        if (ImGui::DragInt("最大粒子数", &maxP, 10, 1, 50000)) {
            data.maxParticles = static_cast<uint32_t>((std::max)(1, maxP));
            emitter->SetData(data);
        }
        if (ImGui::IsItemActivated()) context->PushUndoState("Change Max Particles");

        // 発生レート
        if (ImGui::DragFloat("発生レート (個/秒)", &data.spawnRate, 1.0f, 0.0f, 5000.0f, "%.1f")) {}
        if (ImGui::IsItemActivated()) context->PushUndoState("Change Spawn Rate");

        // 寿命
        if (ImGui::DragFloatRange2("寿命 (秒)", &data.lifetimeMin, &data.lifetimeMax, 0.05f, 0.01f, 30.0f, "Min: %.2f", "Max: %.2f")) {}
        if (ImGui::IsItemActivated()) context->PushUndoState("Change Lifetime");

        // 稼働時間 & 開始遅延
        if (ImGui::DragFloat("エミッター稼働時間", &data.duration, 0.1f, 0.1f, 60.0f, "%.1f秒")) {}
        if (ImGui::IsItemActivated()) context->PushUndoState("Change Duration");

        if (ImGui::DragFloat("開始遅延 (Start Delay)", &data.startDelay, 0.05f, 0.0f, 30.0f, "%.2f秒")) {}
        if (ImGui::IsItemActivated()) context->PushUndoState("Change Delay");

        if (ImGui::Checkbox("ループ再生 (Looping)", &data.isLoop)) {
            context->PushUndoState("Toggle Loop");
        }

        // バースト設定一覧
        ImGui::Separator();
        ImGui::Text("バースト (瞬間一括発生):");
        for (size_t i = 0; i < data.bursts.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));
            float time = data.bursts[i].time;
            int count = static_cast<int>(data.bursts[i].count);
            ImGui::SetNextItemWidth(80);
            if (ImGui::DragFloat("時間", &time, 0.05f, 0.0f, data.duration, "%.2fs")) {
                data.bursts[i].time = time;
            }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(80);
            if (ImGui::DragInt("個数", &count, 1, 1, 10000)) {
                data.bursts[i].count = static_cast<uint32_t>((std::max)(1, count));
            }
            ImGui::SameLine();
            if (ImGui::Button("削除")) {
                context->PushUndoState("Delete Burst");
                data.bursts.erase(data.bursts.begin() + i);
                ImGui::PopID();
                break;
            }
            ImGui::PopID();
        }

        if (ImGui::Button("+ バースト追加")) {
            context->PushUndoState("Add Burst");
            data.bursts.push_back({ 0.0f, 20 });
        }
    }
}

void GPUParticleInspector::DrawShapeSection(GPUParticleEditorContext* context) {
    auto emitter = context->GetSelectedEmitter();
    if (!emitter) return;
    auto& data = emitter->GetData();

    if (ImGui::CollapsingHeader("発生形状 (Spawn Shape)", ImGuiTreeNodeFlags_DefaultOpen)) {
        int shapeIdx = static_cast<int>(data.shape);
        const char* shapeNames[] = { "点 (Point)", "球 (Sphere)", "直方体 (Box)", "円錐 (Cone)", "リング (Ring)" };
        if (ImGui::Combo("形状", &shapeIdx, shapeNames, IM_ARRAYSIZE(shapeNames))) {
            context->PushUndoState("Change Shape");
            data.shape = static_cast<GPUParticleSpawnShape>(shapeIdx);
        }

        if (data.shape == GPUParticleSpawnShape::Sphere || data.shape == GPUParticleSpawnShape::Ring) {
            if (ImGui::DragFloat("半径 (Radius)", &data.shapeRadius, 0.05f, 0.01f, 100.0f, "%.2f")) {}
            if (ImGui::IsItemActivated()) context->PushUndoState("Change Radius");
        } else if (data.shape == GPUParticleSpawnShape::Box) {
            float box[3] = { data.shapeBoxSize.x, data.shapeBoxSize.y, data.shapeBoxSize.z };
            if (ImGui::DragFloat3("直方体サイズ (Size)", box, 0.05f, 0.01f, 100.0f, "%.2f")) {
                data.shapeBoxSize = { box[0], box[1], box[2] };
            }
            if (ImGui::IsItemActivated()) context->PushUndoState("Change Box Size");
        } else if (data.shape == GPUParticleSpawnShape::Cone) {
            if (ImGui::DragFloat("底面半径", &data.shapeConeRadius, 0.05f, 0.01f, 50.0f, "%.2f")) {}
            if (ImGui::IsItemActivated()) context->PushUndoState("Change Cone Radius");
            if (ImGui::DragFloat("円錐角度 (Angle)", &data.shapeConeAngle, 0.5f, 0.0f, 89.0f, "%.1f°")) {}
            if (ImGui::IsItemActivated()) context->PushUndoState("Change Cone Angle");
        }
    }
}

void GPUParticleInspector::DrawPhysicsSection(GPUParticleEditorContext* context) {
    auto emitter = context->GetSelectedEmitter();
    if (!emitter) return;
    auto& data = emitter->GetData();

    if (ImGui::CollapsingHeader("速度 & 物理 (Velocity & Physics)", ImGuiTreeNodeFlags_DefaultOpen)) {
        // 初速範囲
        if (ImGui::DragFloatRange2("初速 (Speed)", &data.initialSpeedMin, &data.initialSpeedMax, 0.1f, 0.0f, 200.0f, "Min: %.1f", "Max: %.1f")) {}
        if (ImGui::IsItemActivated()) context->PushUndoState("Change Speed");

        // 主方向 & 散開角
        float dir[3] = { data.initialVelocityDir.x, data.initialVelocityDir.y, data.initialVelocityDir.z };
        if (ImGui::DragFloat3("射出主方向", dir, 0.05f, -1.0f, 1.0f, "%.2f")) {
            data.initialVelocityDir = { dir[0], dir[1], dir[2] };
        }
        if (ImGui::IsItemActivated()) context->PushUndoState("Change Velocity Dir");

        if (ImGui::DragFloat("散開角度 (Spread)", &data.velocitySpread, 0.5f, 0.0f, 180.0f, "%.1f°")) {}
        if (ImGui::IsItemActivated()) context->PushUndoState("Change Spread");

        // 重力 & 空気抵抗
        if (ImGui::DragFloat("重力 (Gravity Y)", &data.gravity, 0.1f, -100.0f, 100.0f, "%.2f m/s²")) {}
        if (ImGui::IsItemActivated()) context->PushUndoState("Change Gravity");

        if (ImGui::DragFloat("空気抵抗 (Drag)", &data.drag, 0.01f, 0.0f, 20.0f, "%.2f")) {}
        if (ImGui::IsItemActivated()) context->PushUndoState("Change Drag");
    }
}

void GPUParticleInspector::DrawTransformSection(GPUParticleEditorContext* context) {
    auto emitter = context->GetSelectedEmitter();
    if (!emitter) return;
    auto& data = emitter->GetData();

    if (ImGui::CollapsingHeader("サイズ & 回転 (Size & Rotation)", ImGuiTreeNodeFlags_DefaultOpen)) {
        // 初期サイズ範囲
        float minScale[3] = { data.initialScaleMin.x, data.initialScaleMin.y, data.initialScaleMin.z };
        float maxScale[3] = { data.initialScaleMax.x, data.initialScaleMax.y, data.initialScaleMax.z };

        if (ImGui::DragFloat3("初期サイズ Min", minScale, 0.02f, 0.001f, 50.0f, "%.2f")) {
            data.initialScaleMin = { minScale[0], minScale[1], minScale[2] };
        }
        if (ImGui::IsItemActivated()) context->PushUndoState("Change Min Scale");

        if (ImGui::DragFloat3("初期サイズ Max", maxScale, 0.02f, 0.001f, 50.0f, "%.2f")) {
            data.initialScaleMax = { maxScale[0], maxScale[1], maxScale[2] };
        }
        if (ImGui::IsItemActivated()) context->PushUndoState("Change Max Scale");

        // 終了時サイズ倍率
        if (ImGui::DragFloat("終了時サイズ倍率", &data.endScaleFactor, 0.02f, 0.0f, 10.0f, "%.2f倍")) {}
        if (ImGui::IsItemActivated()) context->PushUndoState("Change End Scale Factor");

        ImGui::Separator();

        // 回転速度範囲
        float minRotSpd[3] = { data.rotateSpeedMin.x, data.rotateSpeedMin.y, data.rotateSpeedMin.z };
        float maxRotSpd[3] = { data.rotateSpeedMax.x, data.rotateSpeedMax.y, data.rotateSpeedMax.z };
        if (ImGui::DragFloat3("回転速度 Min", minRotSpd, 0.05f, -50.0f, 50.0f, "%.2f rad/s")) {
            data.rotateSpeedMin = { minRotSpd[0], minRotSpd[1], minRotSpd[2] };
        }
        if (ImGui::IsItemActivated()) context->PushUndoState("Change Min Rot Speed");

        if (ImGui::DragFloat3("回転速度 Max", maxRotSpd, 0.05f, -50.0f, 50.0f, "%.2f rad/s")) {
            data.rotateSpeedMax = { maxRotSpd[0], maxRotSpd[1], maxRotSpd[2] };
        }
        if (ImGui::IsItemActivated()) context->PushUndoState("Change Max Rot Speed");
    }
}

void GPUParticleInspector::DrawColorSection(GPUParticleEditorContext* context) {
    auto emitter = context->GetSelectedEmitter();
    if (!emitter) return;
    auto& data = emitter->GetData();

    if (ImGui::CollapsingHeader("カラー & アルファ (Color & Alpha)", ImGuiTreeNodeFlags_DefaultOpen)) {
        float startCol[4] = { data.startColor.x, data.startColor.y, data.startColor.z, data.startColor.w };
        if (ImGui::ColorEdit4("開始色 (Start Color)", startCol)) {
            data.startColor = { startCol[0], startCol[1], startCol[2], startCol[3] };
        }
        if (ImGui::IsItemActivated()) context->PushUndoState("Change Start Color");

        float endCol[4] = { data.endColor.x, data.endColor.y, data.endColor.z, data.endColor.w };
        if (ImGui::ColorEdit4("終了色 (End Color)", endCol)) {
            data.endColor = { endCol[0], endCol[1], endCol[2], endCol[3] };
        }
        if (ImGui::IsItemActivated()) context->PushUndoState("Change End Color");
    }
}
#endif
