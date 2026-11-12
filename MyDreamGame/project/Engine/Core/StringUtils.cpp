#include "StringUtils.h"
#include <Windows.h>

std::wstring StringUtils::ConvertString(const std::string& str) {
    // 空文字の場合はすぐに空文字を返す
    if (str.empty()) {
        return std::wstring();
    }
    
    // UTF-8 -> UTF-16 変換
    auto sizeNeeded = MultiByteToWideChar(
        CP_UTF8, 0, reinterpret_cast<const char *>(&str[0]), static_cast<int>(str.size()),NULL,0);
    // 変換サイズが0の場合は空文字を返す
    if (sizeNeeded == 0) {
        return std::wstring();
    }

    // 必用なサイズ分のバッファを確保して実際に変換
    std::wstring result(sizeNeeded, 0);
    MultiByteToWideChar(
        CP_UTF8, 0, reinterpret_cast<const char *>(&str[0]), static_cast<int>(str.size()), &result[0], sizeNeeded);

    return result;
}

std::string StringUtils::ConvertString(const std::wstring &str) {
    // 空文字の場合はすぐに空文字を返す
    if (str.empty()) {
        return std::string();
    }
    // UTF-16 -> UTF-8 変換
    auto sizeNeeded = WideCharToMultiByte(
        CP_UTF8, 0, str.data(), static_cast<int>(str.size()), NULL, 0, NULL, NULL);
    // 変換サイズが0の場合は空文字を返す
    if (sizeNeeded == 0) {
        return std::string();
    }
    // 必用なサイズ分のバッファを確保して実際に変換
    std::string result(sizeNeeded, 0);
    WideCharToMultiByte(
        CP_UTF8, 0, str.data(), static_cast<int>(str.size()), &result[0], sizeNeeded, NULL, NULL);
    return result;
}   



