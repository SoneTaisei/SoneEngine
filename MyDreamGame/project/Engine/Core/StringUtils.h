#pragma once
#include <string>

class StringUtils {
public:
    // string -> wstring 変換
    static std::wstring ConvertString(const std::string& str);

    // wstring -> string 変換
    static std::string ConvertString(const std::wstring& str);
};
