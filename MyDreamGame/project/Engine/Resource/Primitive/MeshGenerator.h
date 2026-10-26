#pragma once
#include <vector>
#include <cstdint>
#include "Core/Utility/Structs.h"

class MeshGenerator {
public:
    /// <summary>
    /// 球体（Sphere）の頂点データとインデックスデータを生成する
    /// </summary>
    /// <param name="vertices">生成された頂点を追加するバッファ</param>
    /// <param name="indices">生成されたインデックスを追加するバッファ</param>
    /// <param name="radius">球の半径</param>
    /// <param name="latDiv">緯度方向（縦）の分割数</param>
    /// <param name="lonDiv">経度方向（横）の分割数</param>
    static void CreateSphereMesh(
        std::vector<VertexData> &vertices,
        std::vector<uint32_t> &indices,
        float radius,
        int latDiv,
        int lonDiv);
    /// <summary>
    /// スカイボックス用立方体（Box）の頂点データとインデックスデータを生成する
    /// </summary>
    /// <param name="vertices">生成されたスカイボックス頂点を格納するバッファ（24頂点）</param>
    /// <param name="indices">生成されたインデックスを追加するバッファ（36インデックス）</param>
    static void CreateBoxMesh(
        std::vector<SkyboxVertexData> &vertices,
        std::vector<uint32_t> &indices);
};
