#pragma once
#include "Core/Utility/Structs.h"
#include <string>

/// <summary>
/// 3Dモデルデータの独自バイナリキャッシュ入出力クラス
/// glTF等のパース済みModelDataを高速にディスク保存・復元し、Assimpのパース負荷をスキップします。
/// </summary>
class ModelBinaryIO {
public:
    /// <summary>
    /// ModelData をバイナリファイル（.modelbin）に保存する
    /// </summary>
    static bool Save(const std::string &binFilePath, const ModelData &modelData);

    /// <summary>
    /// バイナリファイル（.modelbin）から ModelData を高速に読み込む
    /// </summary>
    static bool Load(const std::string &binFilePath, ModelData &outModelData);

    /// <summary>
    /// 元モデルファイル（.gltf等）に対応する最新のバイナリキャッシュが存在するか確認し、
    /// 有効であればロードして true を返す。
    /// </summary>
    static bool TryLoadCache(const std::string &originalModelPath, ModelData &outModelData);

    /// <summary>
    /// 元モデルファイルに対応するバイナリキャッシュ（.modelbin）を保存する
    /// </summary>
    static void SaveCache(const std::string &originalModelPath, const ModelData &modelData);
};
