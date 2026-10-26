#pragma once
#include <string>
#include "Core/Utility/Structs.h"

/// <summary>
/// 3Dモデルファイルの読み込み、解析、キャッシュ管理を行うクラス
/// </summary>
class ModelLoader {
public:
    ///< summary>
    /// 3Dモデルファイルを読み込み、解析してModelData構造体に格納する
    /// </summary>
    /// <param name="directoryPath">モデルファイルのディレクトリパス</param>
    /// <param name="filename">モデルファイル名</param>
    /// <returns>解析されたModelData構造体</returns>
    static ModelData LoadModelFile(const std::string &directoryPath, const std::string &filename);
};
