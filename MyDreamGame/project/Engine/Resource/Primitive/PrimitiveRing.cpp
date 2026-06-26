#include "PrimitiveRing.h"
#include "Core/Utility/UtilityFunctions.h"
#include "Core/Utility/TransformFunctions.h"
#include <cmath>
#include <numbers>

PrimitiveRing::PrimitiveRing(float innerRadius, float outerRadius, uint32_t segments, float startAngle, float endAngle, const Vector4& innerColor, const Vector4& outerColor, bool isRadialUV) : innerRadius_(innerRadius), outerRadius_(outerRadius), segments_(segments), startAngle_(startAngle), endAngle_(endAngle), innerColor_(innerColor), outerColor_(outerColor), isRadialUV_(isRadialUV) {
}

void PrimitiveRing::GenerateModelData() {

    float angleRange = endAngle_ - startAngle_;

    for (uint32_t i = 0; i < segments_; ++i) {
        float ratioCurrent = (float)i / segments_;
        float ratioNext = (float)(i + 1) / segments_;
        
        float angleCurrent = startAngle_ + ratioCurrent * angleRange;
        float angleNext = startAngle_ + ratioNext * angleRange;

        float sinCurrent = sinf(angleCurrent);
        float cosCurrent = cosf(angleCurrent);
        float sinNext = sinf(angleNext);
        float cosNext = cosf(angleNext);

        VertexData vertices[4];

        // 資料に基づいた座標計算 (-sin, cos)
        vertices[0].position = { -sinCurrent * outerRadius_, cosCurrent * outerRadius_, 0.0f, 1.0f };
        vertices[1].position = { -sinNext * outerRadius_, cosNext * outerRadius_, 0.0f, 1.0f };
        vertices[2].position = { -sinCurrent * innerRadius_, cosCurrent * innerRadius_, 0.0f, 1.0f };
        vertices[3].position = { -sinNext * innerRadius_, cosNext * innerRadius_, 0.0f, 1.0f };

        // 頂点カラーの設定
        vertices[0].color = outerColor_;
        vertices[1].color = outerColor_;
        vertices[2].color = innerColor_;
        vertices[3].color = innerColor_;

        // UV座標の設定
        if (isRadialUV_) {
            // Vertical (Radial) UV: V方向が半径方向 (内側 1.0, 外側 0.0)
            vertices[0].texcoord = { ratioCurrent, 0.0f };
            vertices[1].texcoord = { ratioNext, 0.0f };
            vertices[2].texcoord = { ratioCurrent, 1.0f };
            vertices[3].texcoord = { ratioNext, 1.0f };
        } else {
            // Horizontal (Circular) UV: U方向が円周方向 (0.0 ~ 1.0)
            vertices[0].texcoord = { ratioCurrent, 0.0f };
            vertices[1].texcoord = { ratioNext, 0.0f };
            vertices[2].texcoord = { ratioCurrent, 1.0f };
            vertices[3].texcoord = { ratioNext, 1.0f };
            // Note: 現在の実装では Circular がデフォルト
        }

        // 法線の設定
        for (int j = 0; j < 4; ++j) {
            vertices[j].normal = { 0.0f, 0.0f, 1.0f };
        }

        uint32_t baseIndex = static_cast<uint32_t>(modelData_.vertices.size());
        for (int j = 0; j < 4; ++j) {
            modelData_.vertices.push_back(vertices[j]);
        }

        modelData_.indices.push_back(baseIndex + 2); // 内周(現)
        modelData_.indices.push_back(baseIndex + 0); // 外周(現)
        modelData_.indices.push_back(baseIndex + 1); // 外周(次)

        modelData_.indices.push_back(baseIndex + 2); // 内周(現)
        modelData_.indices.push_back(baseIndex + 1); // 外周(次)
        modelData_.indices.push_back(baseIndex + 3); // 内周(次)
    }

}
