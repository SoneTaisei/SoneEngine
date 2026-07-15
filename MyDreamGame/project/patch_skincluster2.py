import os

cpp_path = 'c:/授業/学年/3年前期/自作エンジン/MyDreamGame/project/Engine/Core/Utility/UtilityFunctions.cpp'
h_path = 'c:/授業/学年/3年前期/自作エンジン/MyDreamGame/project/Engine/Core/Utility/UtilityFunctions.h'

cpp_code = r'''
void Update(SkinCluster& skinCluster, const Skeleton& skeleton) {
    for (size_t jointIndex = 0; jointIndex < skeleton.joints.size(); ++jointIndex) {
        assert(jointIndex < skinCluster.inverseBindPoseMatrices.size());
        
        Matrix4x4 inverseBindPose = skinCluster.inverseBindPoseMatrices[jointIndex];
        Matrix4x4 skeletonSpaceMatrix = skeleton.joints[jointIndex].skeletonSpaceMatrix;

        // パレットに格納する行列 = inverseBindPose * skeletonSpaceMatrix
        Matrix4x4 paletteMatrix = inverseBindPose * skeletonSpaceMatrix;
        skinCluster.mappedPalette[jointIndex].skeletonSpaceMatrix = paletteMatrix;
        
        // 法線用の逆転置行列 (スケールが含まれていなければ paletteMatrix と同一でもよいが厳密には逆転置)
        // 簡易的に paletteMatrix をそのまま送るか、Transpose(Inverse(paletteMatrix)) を計算する
        // ここでは一旦そのまま
        skinCluster.mappedPalette[jointIndex].skeletonSpaceInverseTransposeMatrix = paletteMatrix;
    }
}
'''

h_code = 'void Update(SkinCluster& skinCluster, const Skeleton& skeleton);\n'

with open(cpp_path, 'a', encoding='utf-8') as f:
    f.write('\n' + cpp_code + '\n')

with open(h_path, 'a', encoding='utf-8') as f:
    f.write(h_code)

print("Added Update(SkinCluster...)")
