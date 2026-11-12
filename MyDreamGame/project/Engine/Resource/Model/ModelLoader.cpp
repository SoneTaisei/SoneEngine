#include "ModelLoader.h"
#include "Core/Utility/Structs.h"
#include "Core/Utility/TransformFunctions.h"
#include "Resource/Model/ModelBinaryIO.h"
#include "Resource/Texture/TextureLoader.h"
#include <algorithm>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <cctype>
#include <filesystem>
#include <set>
#include <vector>

// ===================================================================
// Assimpのノード階層を再帰的に読み込む内部ヘルパー関数
// ===================================================================
namespace {
Node ReadNode(aiNode *node) {
    Node result;

    aiVector3D scale, translate;
    aiQuaternion rotate;
    node->mTransformation.Decompose(scale, rotate, translate);
    result.transform.scale = {scale.x, scale.y, scale.z};
    result.transform.rotate = {rotate.x, rotate.y, rotate.z, rotate.w};
    result.transform.translate = {translate.x, translate.y, translate.z};
    result.localMatrix = TransformFunctions::MakeAffineMatrix(result.transform.scale, result.transform.rotate, result.transform.translate);

    result.name = node->mName.C_Str();
    result.children.resize(node->mNumChildren);

    for (uint32_t childIndex = 0; childIndex < node->mNumChildren; ++childIndex) {
        result.children[childIndex] = ReadNode(node->mChildren[childIndex]);
    }

    return result;
}
} // namespace

// ===================================================================
// 3Dモデルファイルのロード処理
// ===================================================================
ModelData ModelLoader::LoadModelFile(const std::string &directoryPath, const std::string &filename) {
    ModelData modelData;
    Assimp::Importer importer;
    std::string filePath = directoryPath + "/" + filename;

    // バイナリキャッシュ（.modelbin）が存在し、元モデルより新しければ高速ロード
    if (ModelBinaryIO::TryLoadCache(filePath, modelData)) {
        return modelData;
    }

    // 1. ファイルの読み込み
    const aiScene *scene = nullptr;
    try {
        scene = importer.ReadFile(
            filePath.c_str(),
            aiProcess_Triangulate |
                aiProcess_FlipUVs |
                aiProcess_ConvertToLeftHanded |
                aiProcess_GenSmoothNormals |
                aiProcess_GlobalScale);
    } catch (...) {
        scene = nullptr;
    }

    // メッシュがないファイル（無効なファイル）の場合は空のModelDataを安全に返却
    if (!scene || !scene->HasMeshes() || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !scene->mRootNode) {
        return modelData;
    }

    // 2. マテリアルの解析（テクスチャパスの解決）
    std::vector<std::string> materialTexturePaths(scene->mNumMaterials);
    std::set<std::string> uniqueTextures;
    for (uint32_t materialIndex = 0; materialIndex < scene->mNumMaterials; ++materialIndex) {
        aiMaterial *material = scene->mMaterials[materialIndex];
        if (!material)
            continue;
        aiString textureFilePath;
        std::string resolvedPath;

        auto resolveExistingPath = [&](const std::string &rawName) -> std::string {
            if (rawName.empty())
                return "";
            std::string direct = directoryPath + "/" + rawName;
            if (std::filesystem::exists(direct))
                return direct;

            std::string trimmed = rawName;
            trimmed.erase(0, trimmed.find_first_not_of(" \t\r\n"));
            if (trimmed.find_last_not_of(" \t\r\n") != std::string::npos) {
                trimmed.erase(trimmed.find_last_not_of(" \t\r\n") + 1);
            }
            if (!trimmed.empty() && std::filesystem::exists(directoryPath + "/" + trimmed)) {
                return directoryPath + "/" + trimmed;
            }

            std::string replaced = trimmed;
            for (char &c : replaced) {
                if (c == ' ')
                    c = '_';
            }
            if (std::filesystem::exists(directoryPath + "/" + replaced))
                return directoryPath + "/" + replaced;
            replaced = trimmed;
            for (char &c : replaced) {
                if (c == '_')
                    c = ' ';
            }
            if (std::filesystem::exists(directoryPath + "/" + replaced))
                return directoryPath + "/" + replaced;

            try {
                if (std::filesystem::exists(directoryPath)) {
                    std::string lowerTrimmed = trimmed;
                    std::transform(lowerTrimmed.begin(), lowerTrimmed.end(), lowerTrimmed.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                    for (const auto &entry : std::filesystem::directory_iterator(directoryPath)) {
                        if (!entry.is_regular_file())
                            continue;
                        std::string fn = entry.path().filename().string();
                        std::string lowerFn = fn;
                        std::transform(lowerFn.begin(), lowerFn.end(), lowerFn.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                        if (lowerFn == lowerTrimmed)
                            return entry.path().string();
                    }
                }
            } catch (...) {
            }
            return "";
        };

        if (material->GetTextureCount(aiTextureType_DIFFUSE) != 0) {
            material->GetTexture(aiTextureType_DIFFUSE, 0, &textureFilePath);
            resolvedPath = resolveExistingPath(textureFilePath.C_Str());
        } else if (material->GetTextureCount(aiTextureType_BASE_COLOR) != 0) {
            material->GetTexture(aiTextureType_BASE_COLOR, 0, &textureFilePath);
            resolvedPath = resolveExistingPath(textureFilePath.C_Str());
        }

        if (!resolvedPath.empty() && std::filesystem::exists(resolvedPath)) {
            materialTexturePaths[materialIndex] = resolvedPath;
            uniqueTextures.insert(resolvedPath);
        }
    }

    bool isMultiTextureModel = (uniqueTextures.size() > 1);

    std::vector<aiColor4D> materialColors(scene->mNumMaterials, aiColor4D(1.0f, 1.0f, 1.0f, 1.0f));
    for (uint32_t materialIndex = 0; materialIndex < scene->mNumMaterials; ++materialIndex) {
        aiMaterial *material = scene->mMaterials[materialIndex];
        if (!material)
            continue;

        aiColor4D col(1.0f, 1.0f, 1.0f, 1.0f);
        bool gotColor = false;

        // マルチテクスチャモデルの場合は代表色を抽出して頂点カラーに焼き込む
        const std::string &texFile = materialTexturePaths[materialIndex];
        if (isMultiTextureModel && !texFile.empty()) {
            try {
                DirectX::ScratchImage image = TextureLoader::LoadTexture(texFile);
                const DirectX::Image *img = image.GetImage(0, 0, 0);
                if (img && img->pixels) {
                    DirectX::ScratchImage converted;
                    const DirectX::Image *srcImg = img;
                    if (img->format != DXGI_FORMAT_R32G32B32A32_FLOAT) {
                        if (SUCCEEDED(DirectX::Convert(*img, DXGI_FORMAT_R32G32B32A32_FLOAT, DirectX::TEX_FILTER_DEFAULT, DirectX::TEX_THRESHOLD_DEFAULT, converted))) {
                            srcImg = converted.GetImage(0, 0, 0);
                        }
                    }
                    if (srcImg && srcImg->pixels) {
                        size_t midX = srcImg->width / 2;
                        size_t midY = srcImg->height / 2;
                        const float *pixel = reinterpret_cast<const float *>(srcImg->pixels + (midY * srcImg->rowPitch) + (midX * sizeof(float) * 4));
                        col = aiColor4D(pixel[0], pixel[1], pixel[2], pixel[3]);
                        gotColor = true;
                    }
                }
            } catch (...) {
            }
        }

        if (!gotColor) {
            if (material->Get(AI_MATKEY_BASE_COLOR, col) != AI_SUCCESS) {
                material->Get(AI_MATKEY_COLOR_DIFFUSE, col);
            }
        }
        materialColors[materialIndex] = col;
    }

    // 3. メッシュデータの解析
    for (uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
        aiMesh *mesh = scene->mMeshes[meshIndex];
        if (!mesh)
            continue;

        uint32_t vertexOffset = static_cast<uint32_t>(modelData.vertices.size());
        aiColor4D materialColor = (mesh->mMaterialIndex < materialColors.size())
                                      ? materialColors[mesh->mMaterialIndex]
                                      : aiColor4D(1.0f, 1.0f, 1.0f, 1.0f);

        bool hasNormals = mesh->HasNormals();
        bool hasTexCoords = mesh->HasTextureCoords(0);
        bool hasColors = mesh->HasVertexColors(0);

        // 頂点データの取得
        for (uint32_t vIndex = 0; vIndex < mesh->mNumVertices; ++vIndex) {
            aiVector3D &position = mesh->mVertices[vIndex];
            aiVector3D normal = hasNormals ? mesh->mNormals[vIndex] : aiVector3D(0.0f, 1.0f, 0.0f);
            aiVector3D texcoord = hasTexCoords ? mesh->mTextureCoords[0][vIndex] : aiVector3D(0.0f, 0.0f, 0.0f);

            VertexData vertex;
            vertex.position = {position.x, position.y, position.z, 1.0f};
            vertex.normal = {normal.x, normal.y, normal.z};
            vertex.texcoord = {texcoord.x, texcoord.y};

            if (hasColors) {
                aiColor4D &c = mesh->mColors[0][vIndex];
                vertex.color = {c.r, c.g, c.b, c.a};
            } else {
                vertex.color = {materialColor.r, materialColor.g, materialColor.b, materialColor.a};
            }
            modelData.vertices.push_back(vertex);
        }

        // ボーン・スキンウェイトの解析
        for (uint32_t boneIndex = 0; boneIndex < mesh->mNumBones; ++boneIndex) {
            aiBone *bone = mesh->mBones[boneIndex];
            if (!bone)
                continue;
            std::string jointName = bone->mName.C_Str();
            JointWeightData &weightData = modelData.skinClusterData[jointName];

            aiMatrix4x4 bindPoseMatrixAssimp = bone->mOffsetMatrix.Inverse();
            aiVector3D scale, translate;
            aiQuaternion rotate;
            bindPoseMatrixAssimp.Decompose(scale, rotate, translate);
            Matrix4x4 bindPoseMatrix = TransformFunctions::MakeAffineMatrix(
                {scale.x, scale.y, scale.z},
                {rotate.x, rotate.y, rotate.z, rotate.w},
                {translate.x, translate.y, translate.z});
            weightData.inverseBindPoseMatrix = TransformFunctions::Inverse(bindPoseMatrix);

            for (uint32_t weightIndex = 0; weightIndex < bone->mNumWeights; ++weightIndex) {
                weightData.vertexWeights.push_back({bone->mWeights[weightIndex].mWeight,
                                                    bone->mWeights[weightIndex].mVertexId + vertexOffset});
            }
        }

        // 面（インデックス）の解析
        for (uint32_t faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex) {
            aiFace &face = mesh->mFaces[faceIndex];
            if (face.mNumIndices == 3) {
                for (uint32_t element = 0; element < face.mNumIndices; ++element) {
                    modelData.indices.push_back(face.mIndices[element] + vertexOffset);
                }
            } else if (face.mNumIndices > 3) {
                for (uint32_t element = 1; element + 1 < face.mNumIndices; ++element) {
                    modelData.indices.push_back(face.mIndices[0] + vertexOffset);
                    modelData.indices.push_back(face.mIndices[element] + vertexOffset);
                    modelData.indices.push_back(face.mIndices[element + 1] + vertexOffset);
                }
            }
        }
    }

    // 4. モデル全体のテクスチャ決定
    if (isMultiTextureModel) {
        modelData.material.textureFilePath = "";
    } else if (uniqueTextures.size() == 1) {
        modelData.material.textureFilePath = *uniqueTextures.begin();
    } else {
        modelData.material.textureFilePath = "";
    }

    // ルートノードの再帰読み込み
    if (scene->mRootNode) {
        modelData.rootNode = ReadNode(scene->mRootNode);
    }

    // 次回以降の高速起動のため、バイナリキャッシュ（.modelbin）を保存
    ModelBinaryIO::SaveCache(filePath, modelData);

    return modelData;
}