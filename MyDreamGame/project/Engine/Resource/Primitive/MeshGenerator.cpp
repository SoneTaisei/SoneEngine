#include "MeshGenerator.h"
#define _USE_MATH_DEFINES // M_PI を使用するために必要
#include <cmath>

// ===================================================================
// 球体メッシュの生成
// ===================================================================
void MeshGenerator::CreateSphereMesh(
    std::vector<VertexData> &vertices,
    std::vector<uint32_t> &indices,
    float radius,
    int latDiv,
    int lonDiv) {

    // --- 1. 頂点データの生成（緯度・経度の2重ループ） ---
    // 緯度の角度 θ (0 ~ π)
    for (int lat = 0; lat <= latDiv; ++lat) {
        float theta = lat * float(M_PI) / float(latDiv);
        float sinTheta = sinf(theta);
        float cosTheta = cosf(theta);

        // 経度の角度 φ (0 ~ 2π)
        for (int lon = 0; lon <= lonDiv; ++lon) {
            float phi = lon * 2.0f * float(M_PI) / float(lonDiv);
            float sinPhi = sinf(phi);
            float cosPhi = cosf(phi);

            // 球面上の正規化座標 (x, y, z) を三角関数で計算
            float x = cosPhi * sinTheta;
            float y = cosTheta;
            float z = sinPhi * sinTheta;

            // 頂点情報を作成
            VertexData v{};
            // 半径を乗算して位置を設定
            v.position = {radius * x, radius * y, radius * z, 1.0f};
            // 球面の法線は中心から表面へのベクトル（正規化済み）
            v.normal = {v.position.x / radius, v.position.y / radius, v.position.z / radius};
            // UV座標（0.0f ~ 1.0f にマッピング）
            v.texcoord = {(float)lon / lonDiv, (float)lat / latDiv};
            v.color = {1.0f, 1.0f, 1.0f, 1.0f};

            vertices.push_back(v);
        }
    }

    // --- 2. 三角形インデックスの生成（四角形パッチを2つの三角形に分割） ---
    for (int lat = 0; lat < latDiv; ++lat) {
        for (int lon = 0; lon < lonDiv; ++lon) {
            // 現在のグリッドの頂点インデックス番号を計算
            int first = lat * (lonDiv + 1) + lon;
            int second = first + lonDiv + 1;

            // 三角形1（左下 -> 右下 -> 左上）
            indices.push_back(first);
            indices.push_back(first + 1);
            indices.push_back(second);

            // 三角形2（左上 -> 右下 -> 右上）
            indices.push_back(second);
            indices.push_back(first + 1);
            indices.push_back(second + 1);
        }
    }
}

// ===================================================================
// スカイボックス用ボックスメッシュの生成
// ===================================================================
void MeshGenerator::CreateBoxMesh(
    std::vector<SkyboxVertexData> &vertices,
    std::vector<uint32_t> &indices) {

    // 6面 × 4頂点 = 計24頂点
    vertices.resize(24);

    // --- 各面の頂点座標の定義（中心原点、サイズ 2x2x2 の立方体） ---
    // 右面 (+X)
    vertices[0].position = {1.0f, 1.0f, 1.0f, 1.0f};
    vertices[1].position = {1.0f, 1.0f, -1.0f, 1.0f};
    vertices[2].position = {1.0f, -1.0f, 1.0f, 1.0f};
    vertices[3].position = {1.0f, -1.0f, -1.0f, 1.0f};
    // 左面 (-X)
    vertices[4].position = {-1.0f, 1.0f, -1.0f, 1.0f};
    vertices[5].position = {-1.0f, 1.0f, 1.0f, 1.0f};
    vertices[6].position = {-1.0f, -1.0f, -1.0f, 1.0f};
    vertices[7].position = {-1.0f, -1.0f, 1.0f, 1.0f};
    // 前面 (+Z)
    vertices[8].position = {-1.0f, 1.0f, 1.0f, 1.0f};
    vertices[9].position = {1.0f, 1.0f, 1.0f, 1.0f};
    vertices[10].position = {-1.0f, -1.0f, 1.0f, 1.0f};
    vertices[11].position = {1.0f, -1.0f, 1.0f, 1.0f};
    // 後面 (-Z)
    vertices[12].position = {1.0f, 1.0f, -1.0f, 1.0f};
    vertices[13].position = {-1.0f, 1.0f, -1.0f, 1.0f};
    vertices[14].position = {1.0f, -1.0f, -1.0f, 1.0f};
    vertices[15].position = {-1.0f, -1.0f, -1.0f, 1.0f};
    // 上面 (+Y)
    vertices[16].position = {-1.0f, 1.0f, -1.0f, 1.0f};
    vertices[17].position = {1.0f, 1.0f, -1.0f, 1.0f};
    vertices[18].position = {-1.0f, 1.0f, 1.0f, 1.0f};
    vertices[19].position = {1.0f, 1.0f, 1.0f, 1.0f};
    // 下面 (-Y)
    vertices[20].position = {-1.0f, -1.0f, 1.0f, 1.0f};
    vertices[21].position = {1.0f, -1.0f, 1.0f, 1.0f};
    vertices[22].position = {-1.0f, -1.0f, -1.0f, 1.0f};
    vertices[23].position = {1.0f, -1.0f, -1.0f, 1.0f};

    // --- インデックスの定義（6面 × 2ポリゴン = 12三角形） ---
    // 各面 [0,1,2] と [2,1,3] のパターンで計6面分を追加
    for (uint32_t i = 0; i < 6; ++i) {
        uint32_t offset = i * 4;
        indices.push_back(offset + 0);
        indices.push_back(offset + 1);
        indices.push_back(offset + 2);
        indices.push_back(offset + 2);
        indices.push_back(offset + 1);
        indices.push_back(offset + 3);
    }
}