import os

cpp_path = 'c:/授業/学年/3年前期/自作エンジン/MyDreamGame/project/Engine/Core/Utility/UtilityFunctions.cpp'
h_path = 'c:/授業/学年/3年前期/自作エンジン/MyDreamGame/project/Engine/Core/Utility/UtilityFunctions.h'

cpp_code = r'''
SkinCluster CreateSkinCluster(Microsoft::WRL::ComPtr<ID3D12Device> device, const Skeleton& skeleton, const ModelData& modelData) {
    SkinCluster skinCluster;

    // paletteResourceの生成 (関節数分のマトリックス配列)
    skinCluster.paletteResource = CreateBufferResource(device, sizeof(WellForGPU) * skeleton.joints.size());
    WellForGPU* mappedPalette = nullptr;
    skinCluster.paletteResource->Map(0, nullptr, reinterpret_cast<void**>(&mappedPalette));
    skinCluster.mappedPalette = {mappedPalette, skeleton.joints.size()};

    // SRVの生成
    // SrvManagerを使う場合は引数でもらうか、GetInstance()を叩く
    // ここでは宣言のみとし、SRV割り当てが必要ならSrvManagerを利用する
    // #include "Renderer/SrvManager.h" 等が必要だが、SrvManagerへの依存を避ける場合は呼び出し側でSRVを作る

    // influenceResourceの生成 (頂点ごとのウェイトデータ)
    skinCluster.influenceResource = CreateBufferResource(device, sizeof(VertexWeightData) * modelData.vertices.size());
    VertexWeightData* mappedInfluence = nullptr;
    skinCluster.influenceResource->Map(0, nullptr, reinterpret_cast<void**>(&mappedInfluence));
    std::memset(mappedInfluence, 0, sizeof(VertexWeightData) * modelData.vertices.size());
    skinCluster.mappedInfluence = {mappedInfluence, modelData.vertices.size()};

    skinCluster.influenceBufferView.BufferLocation = skinCluster.influenceResource->GetGPUVirtualAddress();
    skinCluster.influenceBufferView.SizeInBytes = UINT(sizeof(VertexWeightData) * modelData.vertices.size());
    skinCluster.influenceBufferView.StrideInBytes = sizeof(VertexWeightData);

    // InverseBindPoseMatrix の保存
    skinCluster.inverseBindPoseMatrices.resize(skeleton.joints.size());
    for (size_t i = 0; i < skeleton.joints.size(); ++i) {
        skinCluster.inverseBindPoseMatrices[i] = Matrix4x4::Identity(); // デフォルト
    }

    // ウェイト情報のパース
    for (const auto& jointWeight : modelData.skinClusterData) {
        auto it = skeleton.jointMap.find(jointWeight.first);
        if (it == skeleton.jointMap.end()) continue; // そのボーンは存在しない

        int32_t jointIndex = it->second;
        skinCluster.inverseBindPoseMatrices[jointIndex] = jointWeight.second.inverseBindPoseMatrix;

        for (const auto& weightInfo : jointWeight.second.vertexWeights) {
            uint32_t vIndex = weightInfo.vertexIndex;
            if (vIndex >= modelData.vertices.size()) continue;

            // 空いているウェイトスロットを探す
            for (uint32_t slot = 0; slot < kNumMaxInfluence; ++slot) {
                if (skinCluster.mappedInfluence[vIndex].weight[slot] == 0.0f) {
                    skinCluster.mappedInfluence[vIndex].weight[slot] = weightInfo.weight;
                    skinCluster.mappedInfluence[vIndex].jointIndex[slot] = jointIndex;
                    break;
                }
            }
        }
    }

    return skinCluster;
}
'''

h_code = 'SkinCluster CreateSkinCluster(Microsoft::WRL::ComPtr<ID3D12Device> device, const Skeleton& skeleton, const ModelData& modelData);\n'

with open(cpp_path, 'a', encoding='utf-8') as f:
    f.write('\n' + cpp_code + '\n')

with open(h_path, 'r', encoding='utf-8') as f:
    content = f.read()
    
# h_codeをファイルの最後の#endifの前や末尾に追加
content += '\n' + h_code

with open(h_path, 'w', encoding='utf-8') as f:
    f.write(content)

print("Added CreateSkinCluster")
