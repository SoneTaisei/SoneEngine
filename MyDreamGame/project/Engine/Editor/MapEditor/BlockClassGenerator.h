#pragma once
#include <string>
#include "Core/Utility/Structs.h"

struct BlockClassGenParams {
    std::string className;
    std::string displayName;
    bool isSolid = true;
    bool isOneWay = false;
    Vector4 color = { 0.5f, 0.8f, 0.5f, 1.0f };
};

struct BlockClassGenResult {
    bool success = false;
    std::string message;
    std::string headerPath;
    std::string sourcePath;
};

class BlockClassGenerator {
public:
    /// <summary>
    /// クラス名が有効なC++識別子であるかチェック
    /// </summary>
    static bool IsValidClassName(const std::string& name, std::string& outErrorMessage);

    /// <summary>
    /// 新規ブロッククラス（.h / .cpp）の生成とプロジェクトファイルへの登録
    /// </summary>
    static BlockClassGenResult GenerateBlockClass(const BlockClassGenParams& params);

    /// <summary>
    /// ブロッククラス（.h / .cpp）のファイル削除およびプロジェクトファイルからの登録解除
    /// </summary>
    static bool DeleteBlockClass(const std::string& className, std::string& outMessage);

private:
    static bool AddFileToVcxproj(const std::string& vcxprojPath, const std::string& relativePath, bool isHeader);
    static bool AddFileToFilters(const std::string& filtersPath, const std::string& relativePath, const std::string& filterGroup, bool isHeader);
    static bool RemoveFileFromVcxproj(const std::string& vcxprojPath, const std::string& relativePath);
    static bool RemoveFileFromFilters(const std::string& filtersPath, const std::string& relativePath);
};
