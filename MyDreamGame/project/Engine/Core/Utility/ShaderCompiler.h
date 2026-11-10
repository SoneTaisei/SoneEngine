#pragma once
#include <string>
#include <d3d12.h>
#include <dxcapi.h>
#include <wrl.h>

/// <summary>
/// ShaderCompilerクラス
/// <summary>
class ShaderCompiler {
public:
    /// <summary>
    /// HLSLシェーダーファイルをコンパイルし、GPU実行用バイナリBlobを取得する
    /// <summary>
    /// <param name="filePath">コンパイル対象のHLSLファイルパス</param>
    /// <param name="profile">シェーダープロファイル (例: L"vs_6_0", L"ps_6_0")</param>
    /// <param name="dxcUtils">DXCユーティリティポインタ</param>
    /// <param name="dxcCompiler">DXCコンパイラポインタ</param>
    /// <param name="includeHandler">インクルードハンドラポインタ</param>
    /// <returns>コンパイルされたバイナリBlob (IDxcBlob*)</returns>
    static IDxcBlob *CompileShader(
        const std::wstring &filePath,
        const wchar_t *profile,
        IDxcUtils *dxcUtils,
        IDxcCompiler3 *dxcCompiler,
        IDxcIncludeHandler *includeHandler);
};
