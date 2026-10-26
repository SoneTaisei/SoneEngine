#include "ShaderCompiler.h"
#include "Core/StringUtils.h"
#include "Core/Utility/UtilityFunctions.h" // Log を利用するため
#include <Windows.h>
#include <cassert>
#include <format>

#pragma comment(lib, "dxcompiler.lib")

// ===================================================================
// HLSLシェーダーのコンパイル処理
// ===================================================================
IDxcBlob *ShaderCompiler::CompileShader(
    const std::wstring &filePath,
    const wchar_t *profile,
    IDxcUtils *dxcUtils,
    IDxcCompiler3 *dxcCompiler,
    IDxcIncludeHandler *includeHandler) {

    // -------------------------------------------------------------
    // 1. HLSLファイルの読み込み
    // -------------------------------------------------------------
    Log(StringUtils::ConvertString(std::format(L"Begin CompileShader, path:{}, profile:{}\n", filePath, profile)));

    IDxcBlobEncoding *shaderSource = nullptr;
    HRESULT hr = dxcUtils->LoadFile(filePath.c_str(), nullptr, &shaderSource);
    if (FAILED(hr)) {
        wchar_t buf[MAX_PATH];
        GetCurrentDirectoryW(MAX_PATH, buf);
        Log(StringUtils::ConvertString(std::format(L"Failed to load shader file: {}. hr: {:08X}, CWD: {}\n", filePath, hr, buf)));
    }
    assert(SUCCEEDED(hr));

    // 読み込んだソースファイル情報をDXCバッファ構造体に設定
    DxcBuffer shaderSourceBuffer{};
    shaderSourceBuffer.Ptr = shaderSource->GetBufferPointer();
    shaderSourceBuffer.Size = shaderSource->GetBufferSize();
    shaderSourceBuffer.Encoding = DXC_CP_UTF8; // UTF-8を明示

    // -------------------------------------------------------------
    // 2. コンパイル引数の設定
    // -------------------------------------------------------------
    LPCWSTR arguments[] = {
        filePath.c_str(), // コンパイル対象のHLSLファイル名
        L"-E",
        L"main", // エントリーポイント（main関数）
        L"-T",
        profile, // シェーダープロファイル (vs_6_0, ps_6_0 など)
        L"-Zi",
        L"-Qembed_debug", // デバッグ情報（シンボル）をバイナリに埋め込む
        L"-Od",           // 最適化を無効化（デバッグしやすくする）
        L"-Zpr",          // 行優先（Row-Major）レイアウト
        L"-HV",
        L"2021", // HLSL2021規則を有効化
    };

    // 実際にコンパイルを実行
    IDxcResult *shaderResult = nullptr;
    hr = dxcCompiler->Compile(
        &shaderSourceBuffer,
        arguments,
        _countof(arguments),
        includeHandler,
        IID_PPV_ARGS(&shaderResult));
    // DXC内部エラー（起動失敗など）のチェック
    assert(SUCCEEDED(hr));

    // -------------------------------------------------------------
    // 3. コンパイル警告・エラーのチェック
    // -------------------------------------------------------------
    IDxcBlobUtf8 *shaderError = nullptr;
    shaderResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&shaderError), nullptr);

    // エラー文字列が存在し、長さが0でなければエラーを出力して中断
    if (shaderError != nullptr && shaderError->GetStringLength() != 0) {
        Log(shaderError->GetStringPointer());
        assert(false);
    }

    // -------------------------------------------------------------
    // 4. コンパイル結果バイナリの取得
    // -------------------------------------------------------------
    IDxcBlob *shaderBlob = nullptr;
    hr = shaderResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
    assert(SUCCEEDED(hr));

    Log(StringUtils::ConvertString(std::format(L"Compile Succeeded, path:{}, profile:{}\n", filePath, profile)));

    // 不要になった中間オブジェクトを解放
    shaderSource->Release();
    shaderResult->Release();

    return shaderBlob;
}