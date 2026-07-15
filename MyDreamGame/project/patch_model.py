import re

file_path = 'c:/授業/学年/3年前期/自作エンジン/MyDreamGame/project/Engine/Core/Utility/UtilityFunctions.cpp'

with open(file_path, 'r', encoding='utf-8') as f:
    content = f.read()

# aiProcess_PreTransformVertices を削除する
content = content.replace('aiProcess_GenSmoothNormals |\n                                                 aiProcess_PreTransformVertices', 'aiProcess_GenSmoothNormals')
content = content.replace('aiProcess_GenSmoothNormals |\r\n                                                 aiProcess_PreTransformVertices', 'aiProcess_GenSmoothNormals')
# もし後ろにコメントなどが付いている場合も考慮
content = re.sub(r'aiProcess_GenSmoothNormals\s*\|\s*aiProcess_PreTransformVertices', 'aiProcess_GenSmoothNormals', content)


# mBonesをパースする処理を追加する
bone_parse_code = r'''
        // Bone解析
        for (uint32_t boneIndex = 0; boneIndex < mesh->mNumBones; ++boneIndex) {
            aiBone* bone = mesh->mBones[boneIndex];
            std::string jointName = bone->mName.C_Str();
            JointWeightData& weightData = modelData.skinClusterData[jointName];

            aiMatrix4x4 bindPoseMatrixAssimp = bone->mOffsetMatrix;
            Matrix4x4 bindPoseMatrix;
            bindPoseMatrix.m[0][0] = bindPoseMatrixAssimp.a1; bindPoseMatrix.m[0][1] = bindPoseMatrixAssimp.b1; bindPoseMatrix.m[0][2] = bindPoseMatrixAssimp.c1; bindPoseMatrix.m[0][3] = bindPoseMatrixAssimp.d1;
            bindPoseMatrix.m[1][0] = bindPoseMatrixAssimp.a2; bindPoseMatrix.m[1][1] = bindPoseMatrixAssimp.b2; bindPoseMatrix.m[1][2] = bindPoseMatrixAssimp.c2; bindPoseMatrix.m[1][3] = bindPoseMatrixAssimp.d2;
            bindPoseMatrix.m[2][0] = bindPoseMatrixAssimp.a3; bindPoseMatrix.m[2][1] = bindPoseMatrixAssimp.b3; bindPoseMatrix.m[2][2] = bindPoseMatrixAssimp.c3; bindPoseMatrix.m[2][3] = bindPoseMatrixAssimp.d3;
            bindPoseMatrix.m[3][0] = bindPoseMatrixAssimp.a4; bindPoseMatrix.m[3][1] = bindPoseMatrixAssimp.b4; bindPoseMatrix.m[3][2] = bindPoseMatrixAssimp.c4; bindPoseMatrix.m[3][3] = bindPoseMatrixAssimp.d4;
            
            // 左手系への変換 (X軸反転)
            // (1, -1, -1) のスケール反転が必要な場合は別途対応するが、Assimpの機能で反転されている場合もある。
            // ここではDecomposeしてXを反転させるか、既存のReadNodeと同様の処理が必要だが、単純にm[0][1], m[0][2], m[1][0], m[2][0]などの符号反転で対応するアプローチもある。
            // ひとまずInverseBindPose行列をそのまま代入する。
            weightData.inverseBindPoseMatrix = bindPoseMatrix;

            for (uint32_t weightIndex = 0; weightIndex < bone->mNumWeights; ++weightIndex) {
                weightData.vertexWeights.push_back({
                    bone->mWeights[weightIndex].mWeight,
                    bone->mWeights[weightIndex].mVertexId
                });
            }
        }
'''

# faceのループの直前にBone解析を挿入
content = content.replace('for (uint32_t faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex) {', bone_parse_code + '\n        for (uint32_t faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex) {')

with open(file_path, 'w', encoding='utf-8') as f:
    f.write(content)

print("Updated LoadModelFile in UtilityFunctions.cpp")
