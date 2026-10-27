#include "TextureConverter.h"
#include "DirectXTex/DirectXTex.h"
#include <filesystem>
#include <algorithm>
#include <Windows.h>

bool TextureConverter::ConvertToDDS(const std::string &inputFilePath, const std::string &outputDdsPath) {
    if (!std::filesystem::exists(inputFilePath)) {
        OutputDebugStringA(("[TextureConverter] Source file does not exist: " + inputFilePath + "\n").c_str());
        return false;
    }

    std::filesystem::path inPath(inputFilePath);
    std::filesystem::path outPath(outputDdsPath);
    std::wstring inPathW = inPath.wstring();
    std::wstring outPathW = outPath.wstring();

    // 1. WIC経由でテクスチャ画像をロード
    DirectX::ScratchImage image;
    DirectX::TexMetadata metadata;
    HRESULT hr = DirectX::LoadFromWICFile(inPathW.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, &metadata, image);
    if (FAILED(hr)) {
        // FORCE_SRGB で失敗した場合は通常フラグで再試行
        hr = DirectX::LoadFromWICFile(inPathW.c_str(), DirectX::WIC_FLAGS_NONE, &metadata, image);
        if (FAILED(hr)) {
            OutputDebugStringA(("[TextureConverter] Failed to load image: " + inputFilePath + "\n").c_str());
            return false;
        }
    }

    // 2. ミップマップチェーンを生成
    DirectX::ScratchImage mipChain;
    hr = DirectX::GenerateMipMaps(
        image.GetImages(), image.GetImageCount(), image.GetMetadata(),
        DirectX::TEX_FILTER_SRGB, 0, mipChain);
    const DirectX::ScratchImage *srcImage = SUCCEEDED(hr) ? &mipChain : &image;

    // 出力先ディレクトリが存在しない場合は作成
    if (outPath.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(outPath.parent_path(), ec);
    }

    // 3. BC7圧縮の準備（入力フォーマットの適合化）
    DirectX::ScratchImage converted;
    const DirectX::ScratchImage *imageForCompress = srcImage;
    DXGI_FORMAT currentFormat = srcImage->GetMetadata().format;
    if (currentFormat != DXGI_FORMAT_R8G8B8A8_UNORM && currentFormat != DXGI_FORMAT_R8G8B8A8_UNORM_SRGB) {
        DXGI_FORMAT convFormat = DirectX::IsSRGB(currentFormat) ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;
        hr = DirectX::Convert(
            srcImage->GetImages(), srcImage->GetImageCount(), srcImage->GetMetadata(),
            convFormat, DirectX::TEX_FILTER_DEFAULT, DirectX::TEX_THRESHOLD_DEFAULT, converted);
        if (SUCCEEDED(hr)) {
            imageForCompress = &converted;
        }
    }

    DXGI_FORMAT targetFormat = DirectX::MakeSRGB(DXGI_FORMAT_BC7_UNORM);
    if (!DirectX::IsSRGB(imageForCompress->GetMetadata().format)) {
        targetFormat = DXGI_FORMAT_BC7_UNORM;
    }

    DirectX::ScratchImage compressed;
    hr = DirectX::Compress(
        imageForCompress->GetImages(), imageForCompress->GetImageCount(), imageForCompress->GetMetadata(),
        targetFormat, DirectX::TEX_COMPRESS_PARALLEL, DirectX::TEX_THRESHOLD_DEFAULT, compressed);

    if (SUCCEEDED(hr)) {
        // BC7圧縮成功: 圧縮テクスチャをDDSとして保存
        hr = DirectX::SaveToDDSFile(
            compressed.GetImages(), compressed.GetImageCount(), compressed.GetMetadata(),
            DirectX::DDS_FLAGS_NONE, outPathW.c_str());
    } else {
        // 圧縮失敗（極小サイズ等）: 非圧縮のままDDSとして保存
        hr = DirectX::SaveToDDSFile(
            srcImage->GetImages(), srcImage->GetImageCount(), srcImage->GetMetadata(),
            DirectX::DDS_FLAGS_NONE, outPathW.c_str());
    }

    if (FAILED(hr)) {
        OutputDebugStringA(("[TextureConverter] Failed to save DDS: " + outputDdsPath + "\n").c_str());
        return false;
    }

    OutputDebugStringA(("[TextureConverter] Converted successfully: " + inputFilePath + " -> " + outputDdsPath + "\n").c_str());
    return true;
}

std::string TextureConverter::GetOrCreateDds(const std::string &originalPath) {
    if (originalPath.empty()) {
        return originalPath;
    }

    std::filesystem::path p(originalPath);
    std::string ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    // 既にDDSの場合はそのまま返却
    if (ext == ".dds") {
        return originalPath;
    }

    // PNG / JPG 等の画像形式でない場合は対象外
    if (ext != ".png" && ext != ".jpg" && ext != ".jpeg" && ext != ".bmp" && ext != ".tga") {
        return originalPath;
    }

    // 元ファイルが存在しない場合はそのまま返却
    if (!std::filesystem::exists(p)) {
        return originalPath;
    }

    // DDSの出力パス（同階層に拡張子を.ddsに変更したもの）
    std::filesystem::path ddsPath = p;
    ddsPath.replace_extension(".dds");
    std::string ddsPathStr = ddsPath.string();

    bool needConvert = false;
    if (!std::filesystem::exists(ddsPath)) {
        needConvert = true;
    } else {
        try {
            auto origTime = std::filesystem::last_write_time(p);
            auto ddsTime = std::filesystem::last_write_time(ddsPath);
            if (origTime > ddsTime) {
                // 元ファイルが更新されているため再変換
                needConvert = true;
            }
        } catch (...) {
            needConvert = true;
        }
    }

    if (needConvert) {
        if (ConvertToDDS(originalPath, ddsPathStr)) {
            return ddsPathStr;
        } else {
            // 変換に失敗した場合は安全のため元パスをフォールバックとして返す
            return originalPath;
        }
    }

    return ddsPathStr;
}
