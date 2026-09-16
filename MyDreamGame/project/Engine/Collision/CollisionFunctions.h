#pragma once
#include "Core/Utility/Vector3.h"
#include <cmath>
#include <numbers>

#ifdef USE_IMGUI
#include "imgui.h"
#endif

/// <summary>
/// 幾何学的な球
/// </summary>
struct SphereShape {
    Vector3 center{ 0.0f, 0.0f, 0.0f };
    float radius = 1.0f;
};

/// <summary>
/// 視野コーン（球面底コーン：サーチライト・視野角判定用）
/// 光源からの直線距離が一定で、3Dスポットライトの描画範囲と完全に一致します。
/// </summary>
struct VisionCone {
    Vector3 eyePosition{ 0.0f, 0.0f, 0.0f };   // 視点（敵の目やライトの座標）
    Vector3 forward{ 0.0f, 0.0f, 1.0f };       // 視線・照射方向（正規化ベクトル）
    float distance = 10.0f;                    // 最大視界距離（ライトの到達距離）
    float halfAngleRad = 0.523598775f;         // 視野角の半角（ラジアン、デフォルト約30度）

    // 度数法での角度設定ヘルパー
    void SetAngleDegree(float halfAngleDeg) {
        halfAngleRad = halfAngleDeg * (std::numbers::pi_v<float> / 180.0f);
    }
    float GetAngleDegree() const {
        return halfAngleRad * (180.0f / std::numbers::pi_v<float>);
    }

    // Y軸回転（3D左右旋回）から前方向を設定（ラジアン）
    void SetForwardFromYaw(float yawRad) {
        forward = { std::sin(yawRad), 0.0f, std::cos(yawRad) };
    }

    // 2.5Dゲーム（XY平面）での旋回角度から前方向を設定（ラジアン）
    void SetForwardFromAngle2D(float angleRad) {
        forward = { std::cos(angleRad), std::sin(angleRad), 0.0f };
    }

    // オイラー角 (Pitch:X, Yaw:Y, Roll:Z) から前方向を設定
    void SetForwardFromEuler(const Vector3& eulerRad);
};

/// <summary>
/// 幾何学的な平面底コーン（数学的な直円錐）
/// </summary>
struct ConeShape {
    Vector3 apex{ 0.0f, 0.0f, 0.0f };          // 頂点（先端）
    Vector3 direction{ 0.0f, 0.0f, 1.0f };      // 軸の向き（頂点から底面への単位ベクトル）
    float height = 10.0f;                       // 高さ
    float radius = 5.0f;                        // 底面半径
};

// ===================================================================================
// 当たり判定関数群
// ===================================================================================

/// <summary>
/// 点 vs 視野コーン（球面底コーン）の交差判定
/// </summary>
bool IsPointInVisionCone(const Vector3& point, const VisionCone& cone);

/// <summary>
/// 球 vs 視野コーン（球面底コーン）の交差判定
/// プレイヤーの球コライダーが敵のサーチライトにかすったかを厳密に判定します。
/// </summary>
bool IsSphereInVisionCone(const Vector3& sphereCenter, float sphereRadius, const VisionCone& cone);
bool IsSphereInVisionCone(const SphereShape& sphere, const VisionCone& cone);

/// <summary>
/// 点 vs 平面底幾何コーンの交差判定
/// </summary>
bool IsPointInCone(const Vector3& point, const ConeShape& cone);

/// <summary>
/// 球 vs 平面底幾何コーンの交差判定
/// </summary>
bool IsSphereInCone(const SphereShape& sphere, const ConeShape& cone);

// ===================================================================================
// デバッグ・GUI用ヘルパー
// ===================================================================================

#ifdef USE_IMGUI
/// <summary>
/// ImGuiで視野コーンのパラメータ（座標・向き・距離・角度）を編集するGUI
/// </summary>
void DrawVisionConeImGui(const char* label, VisionCone& cone);
#endif
