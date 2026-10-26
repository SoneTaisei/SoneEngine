#include "TextureLoader.h"
#include "Core/StringUtils.h"
#include "Core/Utility/UtilityFunctions.h" // CreateBufferResource を利用するため
#include <filesystem>
#include <stdexcept>
#include <vector>

// ===================================================================
// テクスチャファイルの読み込み（フォールバック＆ミップマップ生成付き）
// ===================================================================
DirectX::ScratchImage TextureLoader::LoadTexture(const std::string &filePath) {
    OutputDebugStringA(("TextureLoader::LoadTexture: " + filePath + "\n").c_str());

    DirectX::ScratchImage image{};

    // --- ファイルが存在しない場合の安全なフォールバック（1x1の白テクスチャ） ---
    if (!std::filesystem::exists(filePath)) {
        OutputDebugStringA(("[WARNING] TextureLoader: File not found: " + filePath + ", fallback to 1x1 white texture.\n").c_str());
        HRESULT hrFallback = image.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, 1, 1);
        if (SUCCEEDED(hrFallback)) {
            uint8_t *pixels = image.GetPixels();
            if (pixels) {
                pixels[0] = 255; // R
                pixels[1] = 255; // G
                pixels[2] = 255; // B
                pixels[3] = 255; // A
            }
        }
        return image;
    }

    // パスをワイド文字列に変換
    std::wstring filePathW = StringUtils::ConvertString(filePath);
    HRESULT hr = E_FAIL;

    // --- 拡張子に応じた読み込み ---
    if (filePathW.ends_with(L".dds")) {
        // DDSファイル（sRGB情報が既に入っているためフラグはNONE）
        hr = DirectX::LoadFromDDSFile(filePathW.c_str(), DirectX::DDS_FLAGS_NONE, nullptr, image);
    } else {
        // PNG, JPG, BMPなどのWICファイル（リニア色空間へsRGB補正して読み込む）
        hr = DirectX::LoadFromWICFile(filePathW.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
    }

    // 読み込み失敗時のフォールバック
    if (FAILED(hr)) {
        OutputDebugStringA(("[WARNING] TextureLoader failed to load file: " + filePath + ", fallback to 1x1 white texture.\n").c_str());
        HRESULT hrFallback = image.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, 1, 1);
        if (SUCCEEDED(hrFallback)) {
            uint8_t *pixels = image.GetPixels();
            if (pixels) {
                pixels[0] = 255;
                pixels[1] = 255;
                pixels[2] = 255;
                pixels[3] = 255;
            }
        }
        return image;
    }

    // --- ミップマップの生成 ---
    DirectX::ScratchImage mipImages{};
    if (DirectX::IsCompressed(image.GetMetadata().format)) {
        // BCn系圧縮フォーマットの場合はそのまま使う
        mipImages = std::move(image);
    } else {
        // 非圧縮画像の場合はミップマップを自動生成する（縮小表示時のジャギー軽減）
        hr = DirectX::GenerateMipMaps(
            image.GetImages(), image.GetImageCount(), image.GetMetadata(),
            DirectX::TEX_FILTER_SRGB, 4, mipImages);
        if (FAILED(hr)) {
            return image;
        }
    }

    return mipImages;
}

// ===================================================================
// DirectX12 テクスチャリソースの生成
// ===================================================================
Microsoft::WRL::ComPtr<ID3D12Resource> TextureLoader::CreateTextureResource(
    Microsoft::WRL::ComPtr<ID3D12Device> device,
    const DirectX::TexMetadata &metadata) {

    // メタデータを基にResource設定を作成
    D3D12_RESOURCE_DESC resourceDesc{};
    resourceDesc.Width = UINT(metadata.width);
    resourceDesc.Height = UINT(metadata.height);
    resourceDesc.MipLevels = UINT(metadata.mipLevels);
    resourceDesc.DepthOrArraySize = UINT(metadata.arraySize);
    resourceDesc.Format = metadata.format;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION(metadata.dimension);

    // VRAM上のデフォルトヒープに配置
    D3D12_HEAP_PROPERTIES heapProperties{};
    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

    Microsoft::WRL::ComPtr<ID3D12Resource> resource = nullptr;
    HRESULT hr = device->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_COPY_DEST, // 初期ステートはコピー先
        nullptr,
        IID_PPV_ARGS(&resource));

    if (FAILED(hr)) {
        throw std::runtime_error("TextureLoader::CreateTextureResource failed to create committed resource.");
    }
    return resource;
}

// ===================================================================
// テクスチャデータのGPUアップロード（中間バッファ経由）
// ===================================================================
Microsoft::WRL::ComPtr<ID3D12Resource> TextureLoader::UploadTextureData(
    Microsoft::WRL::ComPtr<ID3D12Resource> texture,
    const DirectX::ScratchImage &mipImages,
    Microsoft::WRL::ComPtr<ID3D12Device> device,
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList) {

    std::vector<D3D12_SUBRESOURCE_DATA> subresources;
    // 読み込んだデータからDirectX12用のSubresource配列を作成
    DirectX::PrepareUpload(device.Get(), mipImages.GetImages(), mipImages.GetImageCount(), mipImages.GetMetadata(), subresources);

    // 中間リソースに必要なメモリサイズを計算
    uint64_t intermediateSize = GetRequiredIntermediateSize(texture.Get(), 0, UINT(subresources.size()));

    // CPUから書き込める中間バッファ（アップロードヒープ）を生成
    Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource = CreateBufferResource(device, intermediateSize);

    // 中間バッファからVRAMのテクスチャリソースへコピーするコマンドを発行
    UpdateSubresources(commandList.Get(), texture.Get(), intermediateResource.Get(), 0, 0, UINT(subresources.size()), subresources.data());

    // テクスチャをシェーダーから読み取り可能なステートへ遷移させる
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = texture.Get();
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
    commandList->ResourceBarrier(1, &barrier);

    // ※ 中間リソースはGPUのコピー実行が終わるまでメモリ上に残す必要があるため返却する
    return intermediateResource;
}