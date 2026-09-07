#pragma once
#include <string>
#include <vector>
#include <array>
#include <cmath>
#include "Core/Utility/Vector3.h"
#include "Core/Utility/Quaternion.h"
#include "Core/Utility/Matrix4x4.h"
#include "Core/Utility/TransformFunctions.h"
#include "Component/AnimatorComponent.h"

// マントの各関節（質点）データ
struct CapeNode {
    std::string boneName;          // ボーン名
    int32_t jointIndex = -1;       // スケルトン内のジョイントIndex
    int32_t parentJointIndex = -1; // 親ジョイントIndex
    Vector3 currentPos{};          // 現在のワールド座標
    Vector3 prevPos{};             // 前フレームのワールド座標
    Vector3 localBindOffset{};     // 親から見た初期相対座標
    float boneLength = 0.1f;       // ボーンの長さ
    bool isInitialized = false;
};

// 1本のマントチェーン（根元から先端までの節）
struct CapeChain {
    std::string chainName;
    std::vector<CapeNode> nodes;
};

// マント物理パラメータ
struct CapeParams {
    float stiffness = 0.28f;              // 復元バネの強さ (0.0: 完全フニャフニャ 〜 1.0: 剛体)
    float damping = 0.85f;                // 速度減衰 (空気抵抗)
    Vector3 gravity{ 0.0f, -5.0f, 0.0f }; // 重力加速度
    float inertiaFactor = 0.7f;           // プレイヤー移動による慣性の強さ
    float windStrength = 0.12f;           // 待機時のそよ風の強さ
};

class CapePhysics {
public:
    CapePhysics() = default;
    ~CapePhysics() = default;

    // 初期化（スケルトンからマントチェーンのノード情報を構築）
    void Initialize(AnimatorComponent* animator);

    // 物理シミュレーション更新＆ボーンへの回転オーバーライド反映
    void Update(AnimatorComponent* animator, const Matrix4x4& modelWorldMatrix, const Vector3& playerVelocity, float deltaTime);

    // パラメータ取得・設定
    CapeParams& GetParams() { return params_; }
    const CapeParams& GetParams() const { return params_; }

    // ワールド位置のリセット（ワープ時や初期化時）
    void Reset(AnimatorComponent* animator, const Matrix4x4& modelWorldMatrix);

private:
    // 2つの単位ベクトル間の回転クォータニオンを計算
    Quaternion FromToRotation(const Vector3& from, const Vector3& to);

    std::vector<CapeChain> chains_;
    CapeParams params_;
    Vector3 prevPlayerPos_{};
    Vector3 playerVelocity_{};
    float windTime_ = 0.0f;
    bool isReady_ = false;
};
