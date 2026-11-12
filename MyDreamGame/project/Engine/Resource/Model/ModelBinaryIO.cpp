#include "ModelBinaryIO.h"
#include <fstream>
#include <filesystem>
#include <Windows.h>

namespace {
    const char kMagic[4] = { 'M', 'D', 'B', '1' };
    const uint32_t kCurrentVersion = 1;

    struct FileHeader {
        char magic[4];
        uint32_t version;
        uint32_t vertexDataSize;
    };

    void WriteString(std::ofstream &ofs, const std::string &str) {
        uint32_t len = static_cast<uint32_t>(str.size());
        ofs.write(reinterpret_cast<const char*>(&len), sizeof(len));
        if (len > 0) {
            ofs.write(str.data(), len);
        }
    }

    bool ReadString(std::ifstream &ifs, std::string &str) {
        uint32_t len = 0;
        if (!ifs.read(reinterpret_cast<char*>(&len), sizeof(len))) return false;
        str.resize(len);
        if (len > 0) {
            if (!ifs.read(&str[0], len)) return false;
        }
        return true;
    }

    void WriteNodeRecursive(std::ofstream &ofs, const Node &node) {
        ofs.write(reinterpret_cast<const char*>(&node.transform), sizeof(node.transform));
        ofs.write(reinterpret_cast<const char*>(&node.localMatrix), sizeof(node.localMatrix));
        WriteString(ofs, node.name);
        uint32_t childCount = static_cast<uint32_t>(node.children.size());
        ofs.write(reinterpret_cast<const char*>(&childCount), sizeof(childCount));
        for (uint32_t i = 0; i < childCount; ++i) {
            WriteNodeRecursive(ofs, node.children[i]);
        }
    }

    bool ReadNodeRecursive(std::ifstream &ifs, Node &node) {
        if (!ifs.read(reinterpret_cast<char*>(&node.transform), sizeof(node.transform))) return false;
        if (!ifs.read(reinterpret_cast<char*>(&node.localMatrix), sizeof(node.localMatrix))) return false;
        if (!ReadString(ifs, node.name)) return false;
        uint32_t childCount = 0;
        if (!ifs.read(reinterpret_cast<char*>(&childCount), sizeof(childCount))) return false;
        node.children.resize(childCount);
        for (uint32_t i = 0; i < childCount; ++i) {
            if (!ReadNodeRecursive(ifs, node.children[i])) return false;
        }
        return true;
    }

    std::string GetCachePath(const std::string &originalModelPath) {
        std::filesystem::path p(originalModelPath);
        p.replace_extension(".modelbin");
        return p.string();
    }
}

bool ModelBinaryIO::Save(const std::string &binFilePath, const ModelData &modelData) {
    std::filesystem::path p(binFilePath);
    if (p.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(p.parent_path(), ec);
    }

    std::ofstream ofs(binFilePath, std::ios::binary);
    if (!ofs.is_open()) return false;

    FileHeader header{};
    std::memcpy(header.magic, kMagic, 4);
    header.version = kCurrentVersion;
    header.vertexDataSize = static_cast<uint32_t>(sizeof(VertexData));
    ofs.write(reinterpret_cast<const char*>(&header), sizeof(header));

    // 1. 頂点配列
    uint32_t vertexCount = static_cast<uint32_t>(modelData.vertices.size());
    ofs.write(reinterpret_cast<const char*>(&vertexCount), sizeof(vertexCount));
    if (vertexCount > 0) {
        ofs.write(reinterpret_cast<const char*>(modelData.vertices.data()), sizeof(VertexData) * vertexCount);
    }

    // 2. インデックス配列
    uint32_t indexCount = static_cast<uint32_t>(modelData.indices.size());
    ofs.write(reinterpret_cast<const char*>(&indexCount), sizeof(indexCount));
    if (indexCount > 0) {
        ofs.write(reinterpret_cast<const char*>(modelData.indices.data()), sizeof(uint32_t) * indexCount);
    }

    // 3. マテリアル情報
    WriteString(ofs, modelData.material.textureFilePath);

    // 4. ノード階層
    WriteNodeRecursive(ofs, modelData.rootNode);

    // 5. スキンクラスタデータ
    uint32_t clusterCount = static_cast<uint32_t>(modelData.skinClusterData.size());
    ofs.write(reinterpret_cast<const char*>(&clusterCount), sizeof(clusterCount));
    for (const auto &[jointName, weightData] : modelData.skinClusterData) {
        WriteString(ofs, jointName);
        ofs.write(reinterpret_cast<const char*>(&weightData.inverseBindPoseMatrix), sizeof(weightData.inverseBindPoseMatrix));
        uint32_t weightCount = static_cast<uint32_t>(weightData.vertexWeights.size());
        ofs.write(reinterpret_cast<const char*>(&weightCount), sizeof(weightCount));
        if (weightCount > 0) {
            ofs.write(reinterpret_cast<const char*>(weightData.vertexWeights.data()), sizeof(VertexWeightInfo) * weightCount);
        }
    }

    return ofs.good();
}

bool ModelBinaryIO::Load(const std::string &binFilePath, ModelData &outModelData) {
    std::ifstream ifs(binFilePath, std::ios::binary);
    if (!ifs.is_open()) return false;

    FileHeader header{};
    if (!ifs.read(reinterpret_cast<char*>(&header), sizeof(header))) return false;
    if (std::memcmp(header.magic, kMagic, 4) != 0) return false;
    if (header.version != kCurrentVersion) return false;
    if (header.vertexDataSize != sizeof(VertexData)) return false;

    // 1. 頂点配列
    uint32_t vertexCount = 0;
    if (!ifs.read(reinterpret_cast<char*>(&vertexCount), sizeof(vertexCount))) return false;
    outModelData.vertices.resize(vertexCount);
    if (vertexCount > 0) {
        if (!ifs.read(reinterpret_cast<char*>(outModelData.vertices.data()), sizeof(VertexData) * vertexCount)) return false;
    }

    // 2. インデックス配列
    uint32_t indexCount = 0;
    if (!ifs.read(reinterpret_cast<char*>(&indexCount), sizeof(indexCount))) return false;
    outModelData.indices.resize(indexCount);
    if (indexCount > 0) {
        if (!ifs.read(reinterpret_cast<char*>(outModelData.indices.data()), sizeof(uint32_t) * indexCount)) return false;
    }

    // 3. マテリアル情報
    if (!ReadString(ifs, outModelData.material.textureFilePath)) return false;

    // 4. ノード階層
    if (!ReadNodeRecursive(ifs, outModelData.rootNode)) return false;

    // 5. スキンクラスタデータ
    uint32_t clusterCount = 0;
    if (!ifs.read(reinterpret_cast<char*>(&clusterCount), sizeof(clusterCount))) return false;
    outModelData.skinClusterData.clear();
    for (uint32_t i = 0; i < clusterCount; ++i) {
        std::string jointName;
        if (!ReadString(ifs, jointName)) return false;
        JointWeightData weightData{};
        if (!ifs.read(reinterpret_cast<char*>(&weightData.inverseBindPoseMatrix), sizeof(weightData.inverseBindPoseMatrix))) return false;
        uint32_t weightCount = 0;
        if (!ifs.read(reinterpret_cast<char*>(&weightCount), sizeof(weightCount))) return false;
        weightData.vertexWeights.resize(weightCount);
        if (weightCount > 0) {
            if (!ifs.read(reinterpret_cast<char*>(weightData.vertexWeights.data()), sizeof(VertexWeightInfo) * weightCount)) return false;
        }
        outModelData.skinClusterData[jointName] = std::move(weightData);
    }

    return true;
}

bool ModelBinaryIO::TryLoadCache(const std::string &originalModelPath, ModelData &outModelData) {
    if (!std::filesystem::exists(originalModelPath)) return false;

    std::string cachePath = GetCachePath(originalModelPath);
    if (!std::filesystem::exists(cachePath)) return false;

    try {
        auto origTime = std::filesystem::last_write_time(originalModelPath);
        auto cacheTime = std::filesystem::last_write_time(cachePath);
        if (origTime > cacheTime) {
            // 元モデルが更新されているためキャッシュは無効
            return false;
        }
    } catch (...) {
        return false;
    }

    if (Load(cachePath, outModelData)) {
        OutputDebugStringA(("[ModelBinaryIO] Loaded model from cache: " + cachePath + "\n").c_str());
        return true;
    }
    return false;
}

void ModelBinaryIO::SaveCache(const std::string &originalModelPath, const ModelData &modelData) {
    if (modelData.vertices.empty()) return;

    std::string cachePath = GetCachePath(originalModelPath);
    if (Save(cachePath, modelData)) {
        OutputDebugStringA(("[ModelBinaryIO] Saved model cache: " + cachePath + "\n").c_str());
    }
}
