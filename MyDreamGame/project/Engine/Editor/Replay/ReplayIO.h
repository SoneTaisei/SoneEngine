#pragma once
#include <string>
#include <vector>

struct ReplayData;
struct ReplayMacro;

class ReplayIO {
public:
    static bool SaveToFile(const ReplayData& data, const std::string& filename);
    static bool LoadFromFile(const std::string& filepath, ReplayData& outData);
    static std::vector<std::string> GetSavedFileList();
    static void DeleteSavedFile(const std::string& filepath);

    static std::vector<ReplayMacro> LoadMacros();
    static void SaveMacros(const std::vector<ReplayMacro>& macros);
};
