#include "LightEditor.h"
#include "Resource/Model/ModelCommon.h"
#include "Graphics/CameraManager.h"
#include "Core/Utility/TransformFunctions.h"
#include "Core/Utility/Quaternion.h"
#include <DirectXMath.h>
#include <fstream>
#include <filesystem>
#include <numbers>
#include <cmath>
#include <algorithm>
#include <nlohmann/json.hpp>

#ifdef USE_IMGUI
#include "../externals/imgui/imgui.h"
#endif

LightEditor::LightEditor() {
    // デフォルトのスポットライトを1つ生成
    SpotLightItem defaultSpot;
    defaultSpot.name = "SpotLight_1";
    defaultSpot.enabled = true;
    defaultSpot.color = {1.0f, 1.0f, 1.0f, 1.0f};
    defaultSpot.position = {0.0f, 0.0f, -5.0f};
    defaultSpot.direction = {0.0f, 0.0f, 1.0f};
    defaultSpot.baseDirection = {0.0f, 0.0f, 1.0f};
    defaultSpot.intensity = 5.0f;
    defaultSpot.distance = 20.0f;
    defaultSpot.decay = 1.5f;
    defaultSpot.angleDeg = 35.0f;
    defaultSpot.falloffDeg = 20.0f;
    defaultSpot.followType = LightFollowType::None;
    defaultSpot.followOffset = {0.0f, 0.0f, 0.0f};
    spotLights_.push_back(defaultSpot);

    Initialize(nullptr);
}

void LightEditor::Initialize(ModelCommon* modelCommon) {
    try {
        std::filesystem::create_directories("resources/json/shared/Lighting");
        // 既存の古い lighting_config.json があれば Lighting ディレクトリへ移行
        if (!std::filesystem::exists("resources/json/shared/Lighting/lighting_config.json") &&
            std::filesystem::exists("resources/json/shared/lighting_config.json")) {
            std::filesystem::copy_file("resources/json/shared/lighting_config.json", "resources/json/shared/Lighting/lighting_config.json");
        }
    } catch (...) {}

    ScanLightFiles();
    LoadLightingConfig(modelCommon);
}

void LightEditor::Update(float deltaTime, ModelCommon* modelCommon, const Vector3* playerPos) {
    if (statusMessageTimer_ > 0.0f) {
        statusMessageTimer_ -= deltaTime;
    }

    if (!modelCommon) return;

    CameraManager* camMgr = CameraManager::GetInstance();
    Vector3 camPos = camMgr ? camMgr->GetCameraPos() : Vector3{0.0f, 0.0f, -10.0f};
    
    // カメラの前方向（視線方向）を取得
    Vector3 camForward = {0.0f, 0.0f, 1.0f};
    if (camMgr) {
        const Matrix4x4& view = camMgr->GetViewMatrix();
        // View行列の逆行列の前方向ベクトル（Z軸成分）
        camForward = TransformFunctions::Normalize(Vector3{view.m[0][2], view.m[1][2], view.m[2][2]});
    }

    // 各スポットライトの追従・アニメーション更新
    for (auto& item : spotLights_) {
        if (!item.enabled) continue;

        switch (item.followType) {
        case LightFollowType::Camera:
            item.position = camPos + item.followOffset;
            item.direction = camForward;
            break;

        case LightFollowType::Player:
            if (playerPos) {
                item.position = *playerPos + item.followOffset;
            }
            break;

        case LightFollowType::RotateAnimation: {
            item.currentAnimAngle += item.rotateSpeed * deltaTime;
            if (item.currentAnimAngle >= 360.0f) item.currentAnimAngle -= 360.0f;
            if (item.currentAnimAngle < 0.0f) item.currentAnimAngle += 360.0f;

            float rad = item.currentAnimAngle * static_cast<float>(std::numbers::pi) / 180.0f;
            float cosTheta = std::cos(rad);
            float sinTheta = std::sin(rad);
            Vector3 a = TransformFunctions::Normalize(item.rotateAxis);
            Vector3 v = item.baseDirection;
            float dotAV = a.x * v.x + a.y * v.y + a.z * v.z;
            Vector3 crossAV = TransformFunctions::Cross(a, v);
            Vector3 rotated = {
                v.x * cosTheta + crossAV.x * sinTheta + a.x * dotAV * (1.0f - cosTheta),
                v.y * cosTheta + crossAV.y * sinTheta + a.y * dotAV * (1.0f - cosTheta),
                v.z * cosTheta + crossAV.z * sinTheta + a.z * dotAV * (1.0f - cosTheta)
            };
            item.direction = TransformFunctions::Normalize(rotated);
            break;
        }

        case LightFollowType::None:
        default:
            break;
        }
    }

    // GPU定数バッファ（ModelCommon）へ最新データを反映
    SpotLightGroup* slGroup = modelCommon->GetSpotLightGroup();
    if (slGroup) {
        slGroup->ambientIntensity = ambientIntensity_;
        slGroup->spotLightCount = static_cast<int32_t>((std::min)(spotLights_.size(), static_cast<size_t>(kMaxSpotLights)));

        for (uint32_t i = 0; i < kMaxSpotLights; ++i) {
            if (i < spotLights_.size()) {
                const auto& item = spotLights_[i];
                slGroup->spotLights[i].color = item.color;
                slGroup->spotLights[i].position = item.position;
                slGroup->spotLights[i].intensity = item.intensity;
                slGroup->spotLights[i].direction = TransformFunctions::Normalize(item.direction);
                slGroup->spotLights[i].distance = item.distance;
                slGroup->spotLights[i].decay = item.decay;
                slGroup->spotLights[i].cosAngle = std::cos(item.angleDeg * static_cast<float>(std::numbers::pi) / 180.0f);
                slGroup->spotLights[i].cosFalloffStart = std::cos(item.falloffDeg * static_cast<float>(std::numbers::pi) / 180.0f);
                slGroup->spotLights[i].enable = item.enabled ? 1 : 0;

                // シャドウマッピング用ビュー射影行列の計算
                Vector3 dir = slGroup->spotLights[i].direction;
                Vector3 eye = item.position;
                Vector3 target = { eye.x + dir.x, eye.y + dir.y, eye.z + dir.z };
                Vector3 up = { 0.0f, 1.0f, 0.0f };
                if (std::abs(dir.y) > 0.99f) {
                    up = { 0.0f, 0.0f, 1.0f };
                }

                DirectX::XMVECTOR eyeV = DirectX::XMVectorSet(eye.x, eye.y, eye.z, 1.0f);
                DirectX::XMVECTOR targetV = DirectX::XMVectorSet(target.x, target.y, target.z, 1.0f);
                DirectX::XMVECTOR upV = DirectX::XMVectorSet(up.x, up.y, up.z, 0.0f);
                DirectX::XMMATRIX viewMat = DirectX::XMMatrixLookAtLH(eyeV, targetV, upV);

                float fovAngle = (item.angleDeg * 2.0f) * static_cast<float>(std::numbers::pi) / 180.0f;
                fovAngle = std::clamp(fovAngle, 0.01f, static_cast<float>(std::numbers::pi) * 0.99f);

                float nearZ = 0.1f;
                float farZ = (item.distance > 0.5f) ? item.distance : 50.0f;
                DirectX::XMMATRIX projMat = DirectX::XMMatrixPerspectiveFovLH(fovAngle, 1.0f, nearZ, farZ);
                DirectX::XMMATRIX viewProjMat = DirectX::XMMatrixMultiply(viewMat, projMat);

                DirectX::XMStoreFloat4x4(reinterpret_cast<DirectX::XMFLOAT4X4*>(&slGroup->spotLights[i].viewProjection), viewProjMat);
                slGroup->spotLights[i].shadowMapIndex = (item.enabled && item.enableShadow) ? 0 : -1;
                slGroup->spotLights[i].shadowBias = item.shadowBias;
                slGroup->spotLights[i].shadowIntensity = item.shadowIntensity;
            } else {
                slGroup->spotLights[i].enable = 0;
                slGroup->spotLights[i].shadowMapIndex = -1;
            }
        }
    }

    // 平行光源 & 点光源の更新
    DirectionalLight* dLight = modelCommon->GetDirectionalLight();
    if (dLight) {
        dLight->color = directionalColor_;
        dLight->direction = TransformFunctions::Normalize(directionalDir_);
        dLight->intensity = enableDirectional_ ? directionalIntensity_ : 0.0f;
        dLight->enableFlatShading = enableFlatShading_ ? 1 : 0;
    }

    PointLight* pLight = modelCommon->GetPointLight();
    if (pLight) {
        pLight->color = pointColor_;
        pLight->position = pointPos_;
        pLight->intensity = enablePoint_ ? pointIntensity_ : 0.0f;
        pLight->radius = pointRadius_;
        pLight->decay = pointDecay_;
    }
}

namespace {
    bool IsPointInsideSpotLightDangerousArea(const Vector3& pt, const SpotLightItem& item) {
        if (!item.enabled || !item.isDangerous) return false;
        
        Vector3 v = { pt.x - item.position.x, pt.y - item.position.y, pt.z - item.position.z };
        float distSq = v.x * v.x + v.y * v.y + v.z * v.z;
        if (distSq > item.distance * item.distance) return false;
        
        float dist = std::sqrt(distSq);
        if (dist < 0.0001f) return true; // 光源位置そのもの
        
        Vector3 dir = item.direction;
        float dirLen = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
        if (dirLen < 0.0001f) return false;
        dir = { dir.x / dirLen, dir.y / dirLen, dir.z / dirLen };
        
        float dotDir = v.x * dir.x + v.y * dir.y + v.z * dir.z;
        if (dotDir <= 0.0f) return false; // 光源の後方
        
        float cosTheta = dotDir / dist;
        
        // 判定基準: 照射全角（外枠・光の届く範囲全体）の内側に触れたら当たり判定（死亡）！
        float cosOuter = std::cos(item.angleDeg * static_cast<float>(std::numbers::pi) / 180.0f);
        return (cosTheta >= cosOuter);
    }
}

bool LightEditor::CheckPlayerHit(const Vector3& playerPos, float playerRadius) const {
    // プレイヤーの高さ（通常 1.6f）を考慮した縦長サンプリング
    float halfH = playerRadius * 2.0f; // 約0.8f
    const Vector3 samples[] = {
        playerPos,
        { playerPos.x, playerPos.y + halfH, playerPos.z },       // 頭頂部
        { playerPos.x, playerPos.y + halfH * 0.5f, playerPos.z },// 胸部
        { playerPos.x, playerPos.y - halfH * 0.5f, playerPos.z },// 腰部
        { playerPos.x, playerPos.y - halfH, playerPos.z },       // 足元
        { playerPos.x - playerRadius, playerPos.y, playerPos.z },
        { playerPos.x + playerRadius, playerPos.y, playerPos.z },
        { playerPos.x - playerRadius, playerPos.y + halfH * 0.5f, playerPos.z },
        { playerPos.x + playerRadius, playerPos.y + halfH * 0.5f, playerPos.z },
        { playerPos.x - playerRadius, playerPos.y - halfH * 0.5f, playerPos.z },
        { playerPos.x + playerRadius, playerPos.y - halfH * 0.5f, playerPos.z },
    };

    for (const auto& item : spotLights_) {
        if (!item.enabled || !item.isDangerous) continue;
        for (const auto& pt : samples) {
            if (IsPointInsideSpotLightDangerousArea(pt, item)) {
                return true;
            }
        }
    }
    return false;
}

bool LightEditor::CheckAABBHit(const AABB2D& aabb) const {
    float xMin = aabb.left;
    float xMax = aabb.right;
    float yMin = aabb.bottom;
    float yMax = aabb.top;

    for (const auto& item : spotLights_) {
        if (!item.enabled || !item.isDangerous) continue;

        // AABBの矩形領域を網羅する 3x5 グリッドサンプリング (頭〜足元、左右)
        for (int iy = 0; iy < 5; ++iy) {
            float ty = static_cast<float>(iy) / 4.0f;
            float y = yMin + (yMax - yMin) * ty;

            for (int ix = 0; ix < 3; ++ix) {
                float tx = static_cast<float>(ix) / 2.0f;
                float x = xMin + (xMax - xMin) * tx;

                const Vector3 pts[] = {
                    { x, y, 0.0f },
                    { x, y, -0.2f },
                    { x, y, 0.2f }
                };

                for (const auto& pt : pts) {
                    if (IsPointInsideSpotLightDangerousArea(pt, item)) {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

bool LightEditor::GetPrimaryShadowViewProjection(Matrix4x4* outViewProj) const {
    if (!outViewProj) return false;
    for (const auto& item : spotLights_) {
        if (item.enabled && item.enableShadow) {
            Vector3 dir = TransformFunctions::Normalize(item.direction);
            Vector3 eye = item.position;
            Vector3 target = { eye.x + dir.x, eye.y + dir.y, eye.z + dir.z };
            Vector3 up = { 0.0f, 1.0f, 0.0f };
            if (std::abs(dir.y) > 0.99f) {
                up = { 0.0f, 0.0f, 1.0f };
            }

            DirectX::XMVECTOR eyeV = DirectX::XMVectorSet(eye.x, eye.y, eye.z, 1.0f);
            DirectX::XMVECTOR targetV = DirectX::XMVectorSet(target.x, target.y, target.z, 1.0f);
            DirectX::XMVECTOR upV = DirectX::XMVectorSet(up.x, up.y, up.z, 0.0f);
            DirectX::XMMATRIX viewMat = DirectX::XMMatrixLookAtLH(eyeV, targetV, upV);

            float fovAngle = (item.angleDeg * 2.0f) * static_cast<float>(std::numbers::pi) / 180.0f;
            fovAngle = std::clamp(fovAngle, 0.01f, static_cast<float>(std::numbers::pi) * 0.99f);

            float nearZ = 0.1f;
            float farZ = (item.distance > 0.5f) ? item.distance : 50.0f;
            DirectX::XMMATRIX projMat = DirectX::XMMatrixPerspectiveFovLH(fovAngle, 1.0f, nearZ, farZ);
            DirectX::XMMATRIX viewProjMat = DirectX::XMMatrixMultiply(viewMat, projMat);

            DirectX::XMStoreFloat4x4(reinterpret_cast<DirectX::XMFLOAT4X4*>(outViewProj), viewProjMat);
            return true;
        }
    }
    return false;
}

void LightEditor::AddSpotLight() {
    if (spotLights_.size() >= kMaxSpotLights) return;

    SpotLightItem newItem;
    newItem.name = "SpotLight_" + std::to_string(spotLights_.size() + 1);
    newItem.enabled = true;
    newItem.isDangerous = true;
    newItem.color = {1.0f, 1.0f, 1.0f, 1.0f};
    newItem.position = {0.0f, 0.0f, -5.0f};
    newItem.direction = {0.0f, 0.0f, 1.0f};
    newItem.baseDirection = {0.0f, 0.0f, 1.0f};
    newItem.intensity = 5.0f;
    newItem.distance = 20.0f;
    newItem.decay = 1.5f;
    newItem.angleDeg = 35.0f;
    newItem.falloffDeg = 20.0f;
    newItem.followType = LightFollowType::None;

    spotLights_.push_back(newItem);
    selectedLightIndex_ = static_cast<int>(spotLights_.size() - 1);
}

void LightEditor::RemoveSpotLight(int index) {
    if (index >= 0 && index < static_cast<int>(spotLights_.size())) {
        spotLights_.erase(spotLights_.begin() + index);
        if (selectedLightIndex_ >= static_cast<int>(spotLights_.size())) {
            selectedLightIndex_ = static_cast<int>(spotLights_.size()) - 1;
        }
    }
}

void LightEditor::DuplicateSpotLight(int index) {
    if (spotLights_.size() >= kMaxSpotLights) return;
    if (index >= 0 && index < static_cast<int>(spotLights_.size())) {
        SpotLightItem copy = spotLights_[index];
        copy.name += "_Copy";
        copy.position.x += 1.0f;
        spotLights_.push_back(copy);
        selectedLightIndex_ = static_cast<int>(spotLights_.size() - 1);
    }
}

namespace {
    Vector3 NormalizeVector(const Vector3& v) {
        float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
        if (len > 0.0001f) {
            return { v.x / len, v.y / len, v.z / len };
        }
        return { 0.0f, -1.0f, 0.0f };
    }

    bool WorldToScreenPos(const Vector3& worldPos, const Matrix4x4& vpMatrix, ImVec2 viewportPos, ImVec2 viewportSize, ImVec2& outScreenPos) {
        float x = worldPos.x * vpMatrix.m[0][0] + worldPos.y * vpMatrix.m[1][0] + worldPos.z * vpMatrix.m[2][0] + vpMatrix.m[3][0];
        float y = worldPos.x * vpMatrix.m[0][1] + worldPos.y * vpMatrix.m[1][1] + worldPos.z * vpMatrix.m[2][1] + vpMatrix.m[3][1];
        float z = worldPos.x * vpMatrix.m[0][2] + worldPos.y * vpMatrix.m[1][2] + worldPos.z * vpMatrix.m[2][2] + vpMatrix.m[3][2];
        float w = worldPos.x * vpMatrix.m[0][3] + worldPos.y * vpMatrix.m[1][3] + worldPos.z * vpMatrix.m[2][3] + vpMatrix.m[3][3];

        if (w <= 0.001f) return false;

        float ndcX = x / w;
        float ndcY = y / w;
        float ndcZ = z / w;

        if (ndcZ < 0.0f || ndcZ > 1.0f) return false;

        outScreenPos.x = viewportPos.x + (ndcX + 1.0f) * 0.5f * viewportSize.x;
        outScreenPos.y = viewportPos.y + (1.0f - ndcY) * 0.5f * viewportSize.y;
        return true;
    }
}

#ifdef USE_IMGUI
void LightEditor::DrawViewport(D3D12_GPU_DESCRIPTOR_HANDLE renderTextureSrvHandle, const Matrix4x4* viewProjMatrix, bool* pOpen) {
    if (pOpen && !*pOpen) return;

    if (ImGui::Begin("ライトエディター", pOpen, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
        DrawViewportContent(renderTextureSrvHandle, viewProjMatrix);
    }
    ImGui::End();
}

void LightEditor::DrawViewportContent(D3D12_GPU_DESCRIPTOR_HANDLE renderTextureSrvHandle, const Matrix4x4* viewProjMatrix) {
    isHovered_ = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);

    // 画面いっぱいにリアルタイム3Dゲーム画面プレビューを描画（ゲームビューと同等）
    ImVec2 contentSize = ImGui::GetContentRegionAvail();
    float aspect = 1280.0f / 720.0f;
    float windowAspect = (contentSize.y > 0.0f) ? (contentSize.x / contentSize.y) : aspect;
    ImVec2 imageSize;
    if (windowAspect > aspect) {
        imageSize.y = contentSize.y;
        imageSize.x = contentSize.y * aspect;
    } else {
        imageSize.x = contentSize.x;
        imageSize.y = (aspect > 0.0f) ? (contentSize.x / aspect) : contentSize.y;
    }

    // 中央寄せ配置
    ImVec2 currentPos = ImGui::GetCursorPos();
    ImVec2 imgPos(currentPos.x + (contentSize.x - imageSize.x) * 0.5f, currentPos.y + (contentSize.y - imageSize.y) * 0.5f);
    ImGui::SetCursorPos(imgPos);
    ImVec2 screenPos = ImGui::GetCursorScreenPos();
    ImGui::Image((ImTextureID)renderTextureSrvHandle.ptr, imageSize);

    // 選択されたスポットライトをワイヤーの四角（3Dバウンディングボックス）で囲んでオーバーレイ表示
    if (viewProjMatrix) {
        DrawOverlay(*viewProjMatrix, screenPos, imageSize);
    }
}

void LightEditor::DrawOverlay(const Matrix4x4& viewProjMatrix, ImVec2 viewportPos, ImVec2 viewportSize, const AABB2D* playerAABB) {
    if (!showDebugCollision_) return;

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    if (!drawList) return;

    drawList->PushClipRect(viewportPos, ImVec2(viewportPos.x + viewportSize.x, viewportPos.y + viewportSize.y), true);

    for (int i = 0; i < (int)spotLights_.size(); i++) {
        const auto& light = spotLights_[i];
        if (!light.enabled) continue;

        bool isSelected = (i == selectedLightIndex_);
        Vector3 p = light.position;
        Vector3 dirNorm = NormalizeVector(light.direction);

        // 1. 光源キューブと方向線
        float halfSize = isSelected ? 0.35f : 0.18f;
        ImU32 boxColor = isSelected ? IM_COL32(255, 230, 0, 255) : (light.isDangerous ? IM_COL32(255, 80, 80, 180) : IM_COL32(180, 180, 180, 140));
        float lineThickness = isSelected ? 2.5f : 1.2f;

        Vector3 v[8] = {
            {p.x - halfSize, p.y - halfSize, p.z - halfSize},
            {p.x + halfSize, p.y - halfSize, p.z - halfSize},
            {p.x + halfSize, p.y + halfSize, p.z - halfSize},
            {p.x - halfSize, p.y + halfSize, p.z - halfSize},
            {p.x - halfSize, p.y - halfSize, p.z + halfSize},
            {p.x + halfSize, p.y - halfSize, p.z + halfSize},
            {p.x + halfSize, p.y + halfSize, p.z + halfSize},
            {p.x - halfSize, p.y + halfSize, p.z + halfSize}
        };

        ImVec2 s[8];
        bool valid[8];
        for (int j = 0; j < 8; j++) {
            valid[j] = WorldToScreenPos(v[j], viewProjMatrix, viewportPos, viewportSize, s[j]);
        }

        const int edges[12][2] = {
            {0, 1}, {1, 2}, {2, 3}, {3, 0},
            {4, 5}, {5, 6}, {6, 7}, {7, 4},
            {0, 4}, {1, 5}, {2, 6}, {3, 7}
        };

        for (int e = 0; e < 12; e++) {
            int idxA = edges[e][0];
            int idxB = edges[e][1];
            if (valid[idxA] && valid[idxB]) {
                drawList->AddLine(s[idxA], s[idxB], boxColor, lineThickness);
            }
        }

        ImVec2 centerScreen;
        if (WorldToScreenPos(p, viewProjMatrix, viewportPos, viewportSize, centerScreen)) {
            ImU32 centerColor = isSelected ? IM_COL32(255, 255, 80, 255) : (light.isDangerous ? IM_COL32(255, 60, 60, 255) : IM_COL32(200, 200, 200, 180));
            drawList->AddCircleFilled(centerScreen, isSelected ? 4.5f : 3.0f, centerColor);

            char label[64];
            snprintf(label, sizeof(label), " [Spot #%d: %s%s]", i + 1, light.name.c_str(), light.isDangerous ? " (危険)" : "");
            drawList->AddText(ImVec2(centerScreen.x + 8, centerScreen.y - 8), isSelected ? IM_COL32(255, 240, 50, 255) : IM_COL32(255, 120, 120, 220), label);
        }

        // 2. スポットライト円錐（Cone）の直交基底ベクトルを計算
        Vector3 up = (std::abs(dirNorm.y) < 0.99f) ? Vector3{0.0f, 1.0f, 0.0f} : Vector3{1.0f, 0.0f, 0.0f};
        Vector3 u = NormalizeVector(TransformFunctions::Cross(dirNorm, up));
        Vector3 w = NormalizeVector(TransformFunctions::Cross(dirNorm, u));

        // 3. プレイヤーが存在する Z=0 平面での光の交差円（判定断面）を描画
        if (std::abs(dirNorm.z) > 0.001f) {
            float tZ0 = -p.z / dirNorm.z;
            if (tZ0 > 0.0f && tZ0 <= light.distance) {
                Vector3 centerZ0 = { p.x + dirNorm.x * tZ0, p.y + dirNorm.y * tZ0, 0.0f };
                float radOuter = tZ0 * std::tan(light.angleDeg * static_cast<float>(std::numbers::pi) / 180.0f);
                float radCore = tZ0 * std::tan(light.falloffDeg * static_cast<float>(std::numbers::pi) / 180.0f);

                const int SEGMENTS = 24;
                ImVec2 prevOuterScreen, prevCoreScreen;
                bool prevOuterValid = false, prevCoreValid = false;

                for (int sIdx = 0; sIdx <= SEGMENTS; ++sIdx) {
                    float phi = static_cast<float>(sIdx) * (2.0f * static_cast<float>(std::numbers::pi) / static_cast<float>(SEGMENTS));
                    float cosP = std::cos(phi);
                    float sinP = std::sin(phi);

                    // 外縁円 (Z=0: 当たり判定境界・光の届く範囲)
                    Vector3 ptOuter = { centerZ0.x + cosP * radOuter, centerZ0.y + sinP * radOuter, 0.0f };
                    ImVec2 scrOuter;
                    bool vOuter = WorldToScreenPos(ptOuter, viewProjMatrix, viewportPos, viewportSize, scrOuter);
                    if (prevOuterValid && vOuter) {
                        ImU32 outerCol = light.isDangerous ? IM_COL32(255, 30, 30, 240) : IM_COL32(255, 220, 80, 140);
                        float outerThick = light.isDangerous ? 2.5f : 1.2f;
                        drawList->AddLine(prevOuterScreen, scrOuter, outerCol, outerThick);
                    }
                    prevOuterScreen = scrOuter;
                    prevOuterValid = vOuter;

                    // 中心コア円 (Z=0: フル輝度エリア)
                    if (radCore > 0.001f) {
                        Vector3 ptCore = { centerZ0.x + cosP * radCore, centerZ0.y + sinP * radCore, 0.0f };
                        ImVec2 scrCore;
                        bool vCore = WorldToScreenPos(ptCore, viewProjMatrix, viewportPos, viewportSize, scrCore);
                        if (prevCoreValid && vCore) {
                            drawList->AddLine(prevCoreScreen, scrCore, IM_COL32(255, 255, 100, 160), 1.2f);
                        }
                        prevCoreScreen = scrCore;
                        prevCoreValid = vCore;
                    }
                }

                // 判定円の中心とラベル
                ImVec2 cz0Screen;
                if (WorldToScreenPos(centerZ0, viewProjMatrix, viewportPos, viewportSize, cz0Screen)) {
                    drawList->AddCircleFilled(cz0Screen, 3.5f, light.isDangerous ? IM_COL32(255, 30, 30, 255) : IM_COL32(255, 200, 50, 200));
                    if (light.isDangerous) {
                        drawList->AddText(ImVec2(cz0Screen.x + 6, cz0Screen.y + 4), IM_COL32(255, 60, 60, 255), "Z=0 光の判定エリア");
                    }
                }
            }
        }
    }

    // 4. プレイヤーの当たり判定ボックス (AABB) の可視化
    if (playerAABB) {
        float xMin = playerAABB->left;
        float xMax = playerAABB->right;
        float yMin = playerAABB->bottom;
        float yMax = playerAABB->top;

        Vector3 pCorners[4] = {
            { xMin, yMax, 0.0f }, // 左上
            { xMax, yMax, 0.0f }, // 右上
            { xMax, yMin, 0.0f }, // 右下
            { xMin, yMin, 0.0f }  // 左下
        };

        ImVec2 pScr[4];
        bool pValid[4];
        for (int j = 0; j < 4; ++j) {
            pValid[j] = WorldToScreenPos(pCorners[j], viewProjMatrix, viewportPos, viewportSize, pScr[j]);
        }

        bool isHit = CheckAABBHit(*playerAABB);
        ImU32 aabbColor = isHit ? IM_COL32(255, 30, 30, 255) : IM_COL32(40, 230, 80, 230);
        float aabbThickness = isHit ? 3.0f : 1.8f;

        for (int j = 0; j < 4; ++j) {
            int next = (j + 1) % 4;
            if (pValid[j] && pValid[next]) {
                drawList->AddLine(pScr[j], pScr[next], aabbColor, aabbThickness);
            }
        }

        // サンプリング点の描画
        for (int iy = 0; iy < 5; ++iy) {
            float ty = static_cast<float>(iy) / 4.0f;
            float y = yMin + (yMax - yMin) * ty;
            for (int ix = 0; ix < 3; ++ix) {
                float tx = static_cast<float>(ix) / 2.0f;
                float x = xMin + (xMax - xMin) * tx;
                ImVec2 ptScr;
                if (WorldToScreenPos(Vector3{x, y, 0.0f}, viewProjMatrix, viewportPos, viewportSize, ptScr)) {
                    drawList->AddCircleFilled(ptScr, 2.5f, aabbColor);
                }
            }
        }

        if (pValid[0]) {
            drawList->AddText(ImVec2(pScr[0].x, pScr[0].y - 16), aabbColor, isHit ? "[HIT! 光判定接触]" : "Player Collider");
        }
    }

    drawList->PopClipRect();
}

void LightEditor::DrawBottomPanel(ModelCommon* modelCommon, bool* pOpen) {
    if (pOpen && !*pOpen) return;

    if (ImGui::Begin("スポットライト", pOpen)) {
        DrawLightEditorUI(modelCommon);
    }
    ImGui::End();
}

void LightEditor::DrawLightEditorUI(ModelCommon* modelCommon) {
    (void)modelCommon;

    // ==========================================
    // 0. ファイル管理セクション（画像準拠）
    // ==========================================
    if (availableLightFiles_.empty()) {
        ScanLightFiles();
        if (availableLightFiles_.empty()) {
            // 初回または空の場合、現在の設定でデフォルトファイルを保存
            SaveToFile("resources/json/shared/Lighting/lighting_config.json");
            ScanLightFiles();
        }
    }

    std::string curFileName = GetCurrentFileName();
    const auto& fileList = availableLightFiles_;

    static std::string lastSyncedFileName = "";
    if (lastSyncedFileName != curFileName) {
        lastSyncedFileName = curFileName;
        strcpy_s(saveFileNameBuf_, sizeof(saveFileNameBuf_), curFileName.c_str());
    }

    // 既存のライティングファイルを選択するコンボボックス (常に表示)
    selectedFileComboIdx_ = -1;
    for (int i = 0; i < static_cast<int>(fileList.size()); ++i) {
        if (fileList[i] == curFileName) {
            selectedFileComboIdx_ = i;
            break;
        }
    }

    std::string comboPreview = (selectedFileComboIdx_ != -1) ? fileList[selectedFileComboIdx_] : (curFileName.empty() ? "ライティングファイルを選択..." : curFileName);
    if (ImGui::BeginCombo("ライティングファイルを選択", comboPreview.c_str())) {
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
        if (fileList.empty()) {
            if (ImGui::Selectable(curFileName.c_str(), true)) {
                strcpy_s(saveFileNameBuf_, sizeof(saveFileNameBuf_), curFileName.c_str());
            }
        }
        ImGui::EndCombo();
    }

    // ファイル名入力 (Enterキーでロード)
    if (ImGui::InputText("ファイル名", saveFileNameBuf_, sizeof(saveFileNameBuf_), ImGuiInputTextFlags_EnterReturnsTrue)) {
        if (strlen(saveFileNameBuf_) > 0) {
            LoadFromFile(saveFileNameBuf_);
        }
    }

    ImGui::Spacing();

    // 操作ボタン (保存、データを削除)
    if (ImGui::Button("保存", ImVec2(100, 26))) {
        if (strlen(saveFileNameBuf_) > 0) {
            SaveToFile(saveFileNameBuf_);
        } else {
            SaveToFile();
        }
    }
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.85f, 0.3f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.6f, 0.1f, 0.1f, 1.0f));
    if (ImGui::Button("データを削除", ImVec2(110, 26))) {
        ImGui::OpenPopup("DeleteLightDataConfirmPopup");
    }
    ImGui::PopStyleColor(3);

    // 削除確認ポップアップ
    if (ImGui::BeginPopupModal("DeleteLightDataConfirmPopup", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        std::string targetFile = GetCurrentFileName();
        ImGui::Text("本当にライティング設定ファイル '%s' を削除しますか？", targetFile.c_str());
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

    // ステータスメッセージ表示
    if (statusMessageTimer_ > 0.0f && !statusMessage_.empty()) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "%s", statusMessage_.c_str());
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::BeginTabBar("LightEditorTabBar")) {

        // ==========================================
        // 1. スポットライト管理タブ
        // ==========================================
        if (ImGui::BeginTabItem("スポットライト (Spot)")) {
            ImGui::Checkbox("当たり判定・ライトコーンを可視化 (Debug Overlay)", &showDebugCollision_);
            ImGui::Separator();

            ImGui::Text("配置済みスポットライト (最大 %d 個): %zu 個", kMaxSpotLights, spotLights_.size());
            
            ImGui::BeginDisabled(spotLights_.size() >= kMaxSpotLights);
            if (ImGui::Button("+ 追加")) {
                AddSpotLight();
            }
            ImGui::EndDisabled();
            
            ImGui::SameLine();
            ImGui::BeginDisabled(spotLights_.empty() || selectedLightIndex_ < 0 || selectedLightIndex_ >= static_cast<int>(spotLights_.size()));
            if (ImGui::Button("複製")) {
                DuplicateSpotLight(selectedLightIndex_);
            }
            ImGui::SameLine();
            if (ImGui::Button("- 削除")) {
                RemoveSpotLight(selectedLightIndex_);
            }
            ImGui::EndDisabled();

            ImGui::Separator();

            // リスト表示
            ImGui::BeginChild("SpotLightList", ImVec2(0, 110), true);
            for (int i = 0; i < static_cast<int>(spotLights_.size()); ++i) {
                ImGui::PushID(i);
                bool enabled = spotLights_[i].enabled;
                if (ImGui::Checkbox("##enabled", &enabled)) {
                    spotLights_[i].enabled = enabled;
                }
                ImGui::SameLine();
                
                std::string label = spotLights_[i].name + (spotLights_[i].enabled ? "" : " (無効)");
                if (ImGui::Selectable(label.c_str(), selectedLightIndex_ == i)) {
                    selectedLightIndex_ = i;
                }
                ImGui::PopID();
            }
            ImGui::EndChild();

            ImGui::Separator();

            // 選択中のスポットライト詳細インスペクター
            if (selectedLightIndex_ >= 0 && selectedLightIndex_ < static_cast<int>(spotLights_.size())) {
                auto& light = spotLights_[selectedLightIndex_];
                ImGui::Text("[詳細設定: %s]", light.name.c_str());

                char nameBuf[64];
                strncpy_s(nameBuf, light.name.c_str(), sizeof(nameBuf));
                if (ImGui::InputText("名前", nameBuf, sizeof(nameBuf))) {
                    light.name = nameBuf;
                }

                ImGui::ColorEdit4("光の色", &light.color.x);
                ImGui::DragFloat("輝度 (Intensity)", &light.intensity, 0.05f, 0.0f, 50.0f, "%.2f");
                ImGui::DragFloat("届く距離 (Distance)", &light.distance, 0.1f, 0.1f, 200.0f, "%.1f");
                ImGui::DragFloat("減衰率 (Decay)", &light.decay, 0.05f, 0.1f, 10.0f, "%.2f");

                if (ImGui::SliderFloat("照射全角 (Angle / 当たり判定範囲)", &light.angleDeg, 1.0f, 90.0f, "%.1f deg")) {
                    if (light.falloffDeg > light.angleDeg) light.falloffDeg = light.angleDeg;
                }
                ImGui::SliderFloat("フォールオフ開始角 (最大輝度芯)", &light.falloffDeg, 0.0f, light.angleDeg, "%.1f deg");

                ImGui::Checkbox("危険な光 (当たるとプレイヤー死亡)", &light.isDangerous);
                ImGui::TextDisabled("※照射全角の内側(光が当たっている領域全体)がプレイヤーの当たり判定になります。");

                ImGui::Separator();
                ImGui::Text("シャドウマッピング (影)");
                ImGui::Checkbox("影を落とす (Enable Shadow)", &light.enableShadow);
                if (light.enableShadow) {
                    ImGui::DragFloat("シャドウバイアス (Bias)", &light.shadowBias, 0.0001f, 0.00001f, 0.05f, "%.5f");
                    ImGui::SliderFloat("影の濃さ (Intensity)", &light.shadowIntensity, 0.0f, 1.0f, "%.2f");
                }

                ImGui::Separator();
                ImGui::Text("追従・動作モード");

                const char* followTypes[] = { "固定 (None)", "カメラ追従 (懐中電灯)", "プレイヤー追従", "自動首振り回転" };
                int currentFollow = static_cast<int>(light.followType);
                if (ImGui::Combo("追従モード", &currentFollow, followTypes, IM_ARRAYSIZE(followTypes))) {
                    light.followType = static_cast<LightFollowType>(currentFollow);
                }

                if (light.followType == LightFollowType::None) {
                    ImGui::DragFloat3("位置", &light.position.x, 0.1f);
                    if (ImGui::DragFloat3("照射方向", &light.direction.x, 0.01f, -1.0f, 1.0f)) {
                        light.direction = TransformFunctions::Normalize(light.direction);
                        light.baseDirection = light.direction;
                    }
                    ImGui::Text("向きプリセット:");
                    ImGui::SameLine();
                    if (ImGui::Button("正面 (奥)")) {
                        light.direction = {0.0f, 0.0f, 1.0f};
                        light.baseDirection = light.direction;
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("下")) {
                        light.direction = {0.0f, -1.0f, 0.0f};
                        light.baseDirection = light.direction;
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("上")) {
                        light.direction = {0.0f, 1.0f, 0.0f};
                        light.baseDirection = light.direction;
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("左")) {
                        light.direction = {-1.0f, 0.0f, 0.0f};
                        light.baseDirection = light.direction;
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("右")) {
                        light.direction = {1.0f, 0.0f, 0.0f};
                        light.baseDirection = light.direction;
                    }
                } else if (light.followType == LightFollowType::Camera || light.followType == LightFollowType::Player) {
                    ImGui::DragFloat3("追従オフセット", &light.followOffset.x, 0.05f);
                    if (light.followType == LightFollowType::Player) {
                        if (ImGui::DragFloat3("照射方向", &light.direction.x, 0.01f, -1.0f, 1.0f)) {
                            light.direction = TransformFunctions::Normalize(light.direction);
                        }
                    }
                } else if (light.followType == LightFollowType::RotateAnimation) {
                    ImGui::DragFloat3("位置", &light.position.x, 0.1f);
                    if (ImGui::DragFloat3("基準照射方向", &light.baseDirection.x, 0.01f, -1.0f, 1.0f)) {
                        light.baseDirection = TransformFunctions::Normalize(light.baseDirection);
                    }
                    ImGui::DragFloat3("回転軸", &light.rotateAxis.x, 0.01f, -1.0f, 1.0f);
                    ImGui::DragFloat("回転速度 (度/秒)", &light.rotateSpeed, 1.0f, -360.0f, 360.0f, "%.1f");
                }
            }

            ImGui::EndTabItem();
        }

        // ==========================================
        // 2. 環境光 & 暗闇設定タブ
        // ==========================================
        if (ImGui::BeginTabItem("環境光・暗闇 (Ambient)")) {
            ImGui::Text("環境光 (アンビエント) 設定");
            ImGui::TextDisabled("※数値を 0.0 に近づけると、画像のような真っ暗なホラー表現になります。");

            ImGui::SliderFloat("環境光の強さ", &ambientIntensity_, 0.0f, 2.0f, "%.3f");

            ImGui::Spacing();
            ImGui::Text("プリセット:");
            if (ImGui::Button("完全な暗闇 (0.00)")) {
                ambientIntensity_ = 0.0f;
            }
            ImGui::SameLine();
            if (ImGui::Button("ホラー探索 (0.05)")) {
                ambientIntensity_ = 0.05f;
            }
            ImGui::SameLine();
            if (ImGui::Button("薄暗い部屋 (0.30)")) {
                ambientIntensity_ = 0.30f;
            }
            ImGui::SameLine();
            if (ImGui::Button("標準 (1.00)")) {
                ambientIntensity_ = 1.0f;
            }

            ImGui::EndTabItem();
        }

        // ==========================================
        // 3. 平行光源 & 点光源タブ
        // ==========================================
        if (ImGui::BeginTabItem("平行光源・点光源")) {
            // 平行光源
            ImGui::Checkbox("平行光源 (Directional) を有効化", &enableDirectional_);
            if (enableDirectional_) {
                ImGui::ColorEdit4("平行光 色", &directionalColor_.x);
                ImGui::DragFloat("平行光 輝度", &directionalIntensity_, 0.02f, 0.0f, 10.0f);
                if (ImGui::DragFloat3("平行光 方向", &directionalDir_.x, 0.01f, -1.0f, 1.0f)) {
                    directionalDir_ = TransformFunctions::Normalize(directionalDir_);
                }
                ImGui::Checkbox("フラットシェーディング", &enableFlatShading_);
            }

            ImGui::Separator();

            // 点光源
            ImGui::Checkbox("点光源 (Point) を有効化", &enablePoint_);
            if (enablePoint_) {
                ImGui::ColorEdit4("点光源 色", &pointColor_.x);
                ImGui::DragFloat("点光源 輝度", &pointIntensity_, 0.02f, 0.0f, 10.0f);
                ImGui::DragFloat3("点光源 位置", &pointPos_.x, 0.1f);
                ImGui::DragFloat("点光源 半径", &pointRadius_, 0.1f, 0.1f, 100.0f);
                ImGui::DragFloat("点光源 減衰", &pointDecay_, 0.02f, 0.1f, 10.0f);
            }

            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }
}
#endif

void LightEditor::NewConfig() {
    ambientIntensity_ = 1.0f;
    enableDirectional_ = true;
    directionalColor_ = {1.0f, 1.0f, 1.0f, 1.0f};
    directionalDir_ = {-0.038f, -0.962f, 0.268f};
    directionalIntensity_ = 1.0f;
    enableFlatShading_ = false;

    enablePoint_ = false;
    pointColor_ = {1.0f, 1.0f, 1.0f, 1.0f};
    pointPos_ = {0.0f, 2.0f, 0.0f};
    pointIntensity_ = 1.0f;
    pointRadius_ = 10.0f;
    pointDecay_ = 1.0f;

    spotLights_.clear();
    SpotLightItem defaultSpot;
    defaultSpot.name = "SpotLight_1";
    defaultSpot.enabled = true;
    defaultSpot.color = {1.0f, 1.0f, 1.0f, 1.0f};
    defaultSpot.position = {0.0f, 0.0f, -5.0f};
    defaultSpot.direction = {0.0f, 0.0f, 1.0f};
    defaultSpot.baseDirection = {0.0f, 0.0f, 1.0f};
    defaultSpot.intensity = 5.0f;
    defaultSpot.distance = 20.0f;
    defaultSpot.decay = 1.5f;
    defaultSpot.angleDeg = 35.0f;
    defaultSpot.falloffDeg = 20.0f;
    defaultSpot.followType = LightFollowType::None;
    defaultSpot.followOffset = {0.0f, 0.0f, 0.0f};
    spotLights_.push_back(defaultSpot);
    selectedLightIndex_ = 0;

    SetStatusMessage("新規ライティング設定を作成しました");
}

std::string LightEditor::StripJsonExtension(const std::string& filename) {
    if (filename.length() >= 5) {
        std::string ext = filename.substr(filename.length() - 5);
        if (ext == ".json" || ext == ".JSON") {
            return filename.substr(0, filename.length() - 5);
        }
    }
    return filename;
}

std::string LightEditor::GetCurrentFileName() const {
    try {
        return std::filesystem::path(currentFilePath_).filename().string();
    } catch (...) {
        return "lighting_config.json";
    }
}

std::string LightEditor::GetCurrentBaseName() const {
    return StripJsonExtension(GetCurrentFileName());
}

std::string LightEditor::ResolveFilePath(const std::string& fileNameOrPath) const {
    if (fileNameOrPath.empty()) {
        return currentFilePath_;
    }

    std::string path = fileNameOrPath;
    std::replace(path.begin(), path.end(), '\\', '/');

    // ディレクトリが含まれていなければ resources/json/shared/Lighting/ を付与
    if (path.find('/') == std::string::npos) {
        path = "resources/json/shared/Lighting/" + path;
    }

    // .json 拡張子が付いていなければ付与
    if (path.length() < 5 || path.substr(path.length() - 5) != ".json") {
        path += ".json";
    }

    return path;
}

void LightEditor::SetCurrentFilePath(const std::string& path) {
    currentFilePath_ = ResolveFilePath(path);
    strcpy_s(saveFileNameBuf_, sizeof(saveFileNameBuf_), GetCurrentFileName().c_str());
    ScanLightFiles();
}

void LightEditor::ScanLightFiles() {
    availableLightFiles_.clear();
    const std::string dir = "resources/json/shared/Lighting";
    try {
        if (!std::filesystem::exists(dir)) {
            std::filesystem::create_directories(dir);
        }
        for (const auto& entry : std::filesystem::directory_iterator(dir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json") {
                availableLightFiles_.push_back(entry.path().filename().string());
            }
        }
        std::sort(availableLightFiles_.begin(), availableLightFiles_.end());
    } catch (...) {
        // ignore errors
    }
}

bool LightEditor::SaveToFile(const std::string& filePath) {
    std::string path = ResolveFilePath(filePath);
    try {
        std::filesystem::path p(path);
        if (p.has_parent_path()) {
            std::filesystem::create_directories(p.parent_path());
        }

        nlohmann::json j;
        j["ambientIntensity"] = ambientIntensity_;
        j["enableDirectional"] = enableDirectional_;
        j["enablePoint"] = enablePoint_;
        j["enableFlatShading"] = enableFlatShading_;

        // Directional
        j["dLight"]["color"] = {directionalColor_.x, directionalColor_.y, directionalColor_.z, directionalColor_.w};
        j["dLight"]["direction"] = {directionalDir_.x, directionalDir_.y, directionalDir_.z};
        j["dIntensity"] = directionalIntensity_;

        // Point
        j["pLight"]["color"] = {pointColor_.x, pointColor_.y, pointColor_.z, pointColor_.w};
        j["pLight"]["position"] = {pointPos_.x, pointPos_.y, pointPos_.z};
        j["pLight"]["radius"] = pointRadius_;
        j["pLight"]["decay"] = pointDecay_;
        j["pIntensity"] = pointIntensity_;

        // SpotLights
        nlohmann::json spotArray = nlohmann::json::array();
        for (const auto& item : spotLights_) {
            nlohmann::json s;
            s["name"] = item.name;
            s["enabled"] = item.enabled;
            s["color"] = {item.color.x, item.color.y, item.color.z, item.color.w};
            s["position"] = {item.position.x, item.position.y, item.position.z};
            s["direction"] = {item.direction.x, item.direction.y, item.direction.z};
            s["baseDirection"] = {item.baseDirection.x, item.baseDirection.y, item.baseDirection.z};
            s["intensity"] = item.intensity;
            s["distance"] = item.distance;
            s["decay"] = item.decay;
            s["angleDeg"] = item.angleDeg;
            s["falloffDeg"] = item.falloffDeg;
            s["isDangerous"] = item.isDangerous;
            s["enableShadow"] = item.enableShadow;
            s["shadowBias"] = item.shadowBias;
            s["shadowIntensity"] = item.shadowIntensity;
            s["followType"] = static_cast<int>(item.followType);
            s["followOffset"] = {item.followOffset.x, item.followOffset.y, item.followOffset.z};
            s["rotateAxis"] = {item.rotateAxis.x, item.rotateAxis.y, item.rotateAxis.z};
            s["rotateSpeed"] = item.rotateSpeed;
            spotArray.push_back(s);
        }
        j["spotLights"] = spotArray;

        std::ofstream ofs(path);
        if (!ofs.is_open()) {
            SetStatusMessage("保存に失敗しました: " + path);
            return false;
        }
        ofs << j.dump(4);
        ofs.close();

        currentFilePath_ = path;
        strcpy_s(saveFileNameBuf_, sizeof(saveFileNameBuf_), GetCurrentFileName().c_str());
        ScanLightFiles();
        SetStatusMessage("保存しました: " + GetCurrentFileName());
        return true;
    } catch (...) {
        SetStatusMessage("保存エラーが発生しました");
        return false;
    }
}

bool LightEditor::LoadFromFile(const std::string& filePath) {
    std::string path = ResolveFilePath(filePath);
    if (!std::filesystem::exists(path)) {
        // フォールバック: shared/lighting_config.json をチェック
        if (std::filesystem::exists("resources/json/shared/lighting_config.json")) {
            path = "resources/json/shared/lighting_config.json";
        } else {
            SetStatusMessage("ファイルが見つかりません: " + path);
            return false;
        }
    }

    try {
        std::ifstream ifs(path);
        if (!ifs.is_open()) {
            SetStatusMessage("ファイルを開けませんでした: " + path);
            return false;
        }

        nlohmann::json j;
        ifs >> j;
        ifs.close();

        if (j.contains("ambientIntensity")) ambientIntensity_ = j["ambientIntensity"];
        if (j.contains("enableDirectional")) enableDirectional_ = j["enableDirectional"];
        if (j.contains("enablePoint")) enablePoint_ = j["enablePoint"];
        if (j.contains("enableFlatShading")) enableFlatShading_ = j["enableFlatShading"];
        if (j.contains("dIntensity")) directionalIntensity_ = j["dIntensity"];
        if (j.contains("pIntensity")) pointIntensity_ = j["pIntensity"];

        if (j.contains("dLight")) {
            if (j["dLight"].contains("color")) {
                directionalColor_ = {j["dLight"]["color"][0], j["dLight"]["color"][1], j["dLight"]["color"][2], j["dLight"]["color"][3]};
            }
            if (j["dLight"].contains("direction")) {
                directionalDir_ = {j["dLight"]["direction"][0], j["dLight"]["direction"][1], j["dLight"]["direction"][2]};
            }
        }

        if (j.contains("pLight")) {
            if (j["pLight"].contains("color")) {
                pointColor_ = {j["pLight"]["color"][0], j["pLight"]["color"][1], j["pLight"]["color"][2], j["pLight"]["color"][3]};
            }
            if (j["pLight"].contains("position")) {
                pointPos_ = {j["pLight"]["position"][0], j["pLight"]["position"][1], j["pLight"]["position"][2]};
            }
            if (j["pLight"].contains("radius")) pointRadius_ = j["pLight"]["radius"];
            if (j["pLight"].contains("decay")) pointDecay_ = j["pLight"]["decay"];
        }

        if (j.contains("spotLights") && j["spotLights"].is_array()) {
            spotLights_.clear();
            for (const auto& item : j["spotLights"]) {
                SpotLightItem sl;
                if (item.contains("name")) sl.name = item["name"];
                if (item.contains("enabled")) sl.enabled = item["enabled"];
                if (item.contains("color")) sl.color = {item["color"][0], item["color"][1], item["color"][2], item["color"][3]};
                if (item.contains("position")) sl.position = {item["position"][0], item["position"][1], item["position"][2]};
                if (item.contains("direction")) sl.direction = {item["direction"][0], item["direction"][1], item["direction"][2]};
                if (item.contains("baseDirection")) sl.baseDirection = {item["baseDirection"][0], item["baseDirection"][1], item["baseDirection"][2]};
                if (item.contains("intensity")) sl.intensity = item["intensity"];
                if (item.contains("distance")) sl.distance = item["distance"];
                if (item.contains("decay")) sl.decay = item["decay"];
                if (item.contains("angleDeg")) sl.angleDeg = item["angleDeg"];
                if (item.contains("falloffDeg")) sl.falloffDeg = item["falloffDeg"];
                if (item.contains("isDangerous")) sl.isDangerous = item["isDangerous"];
                if (item.contains("enableShadow")) sl.enableShadow = item["enableShadow"];
                if (item.contains("shadowBias")) sl.shadowBias = item["shadowBias"];
                if (item.contains("shadowIntensity")) sl.shadowIntensity = item["shadowIntensity"];
                if (item.contains("followType")) sl.followType = static_cast<LightFollowType>(item["followType"]);
                if (item.contains("followOffset")) sl.followOffset = {item["followOffset"][0], item["followOffset"][1], item["followOffset"][2]};
                if (item.contains("rotateAxis")) sl.rotateAxis = {item["rotateAxis"][0], item["rotateAxis"][1], item["rotateAxis"][2]};
                if (item.contains("rotateSpeed")) sl.rotateSpeed = item["rotateSpeed"];
                spotLights_.push_back(sl);
            }
        } else if (j.contains("sLight")) {
            // 旧フォーマット互換
            spotLights_.clear();
            SpotLightItem sl;
            sl.name = "SpotLight";
            sl.enabled = true;
            sl.isDangerous = true;
            if (j["sLight"].contains("color")) sl.color = {j["sLight"]["color"][0], j["sLight"]["color"][1], j["sLight"]["color"][2], j["sLight"]["color"][3]};
            if (j["sLight"].contains("position")) sl.position = {j["sLight"]["position"][0], j["sLight"]["position"][1], j["sLight"]["position"][2]};
            if (j["sLight"].contains("direction")) sl.direction = {j["sLight"]["direction"][0], j["sLight"]["direction"][1], j["sLight"]["direction"][2]};
            if (j["sLight"].contains("distance")) sl.distance = j["sLight"]["distance"];
            if (j["sLight"].contains("decay")) sl.decay = j["sLight"]["decay"];
            if (j.contains("sIntensity")) sl.intensity = j["sIntensity"];
            if (j.contains("spotAngleDeg")) sl.angleDeg = j["spotAngleDeg"];
            if (j.contains("spotFalloffDeg")) sl.falloffDeg = j["spotFalloffDeg"];
            spotLights_.push_back(sl);
        }

        if (spotLights_.empty()) {
            SpotLightItem defaultSpot;
            spotLights_.push_back(defaultSpot);
        }
        selectedLightIndex_ = 0;

        currentFilePath_ = path;
        strcpy_s(saveFileNameBuf_, sizeof(saveFileNameBuf_), GetCurrentFileName().c_str());
        ScanLightFiles();
        SetStatusMessage("読み込みました: " + GetCurrentFileName() + " (" + std::to_string(spotLights_.size()) + " 個)");
        return true;
    } catch (...) {
        SetStatusMessage("JSON読み込みエラーが発生しました");
        return false;
    }
}

bool LightEditor::DeleteFile(const std::string& filePath) {
    std::string path = ResolveFilePath(filePath);
    try {
        if (std::filesystem::exists(path)) {
            std::filesystem::remove(path);
        }
        ScanLightFiles();

        // 削除後、他のファイルが存在すれば先頭をロード、なければ新規作成
        if (!availableLightFiles_.empty()) {
            LoadFromFile(availableLightFiles_[0]);
        } else {
            currentFilePath_ = "resources/json/shared/Lighting/lighting_config.json";
            strcpy_s(saveFileNameBuf_, sizeof(saveFileNameBuf_), GetCurrentFileName().c_str());
            NewConfig();
        }
        SetStatusMessage("ファイルを削除しました: " + StripJsonExtension(std::filesystem::path(path).filename().string()));
        return true;
    } catch (...) {
        SetStatusMessage("ファイル削除エラーが発生しました");
        return false;
    }
}

void LightEditor::SaveLightingConfig(ModelCommon* modelCommon) {
    (void)modelCommon;
    SaveToFile(currentFilePath_);
}

void LightEditor::LoadLightingConfig(ModelCommon* modelCommon) {
    (void)modelCommon;
    LoadFromFile(currentFilePath_);
}
