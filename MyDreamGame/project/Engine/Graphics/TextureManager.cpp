#include "TextureManager.h"
#include "Renderer/DirectXCommon/DirectXCommon.h"
#include "Renderer/SrvManager.h"
#include <cassert>
#include <filesystem>

// Utilityfunctions.cppなどに以下の関数実装があることを想定しています。
// もしmain.cppにしかなければ、そちらから移動させてください。

TextureManager *TextureManager::GetInstance() {
    static TextureManager instance;
    return &instance;
}

void TextureManager::Initialize(Microsoft::WRL::ComPtr<ID3D12Device> device) { // <<< 引数をComPtrに変更
    device_ = device;
}

void TextureManager::Finalize() {
    // ComPtrが自動的にリソースを解放します
    textures_.clear();
    device_ = nullptr;
}

uint32_t TextureManager::Load(const std::string &filePath) {
    auto commandList = DirectXCommon::GetInstance()->GetCommandList();
    
    // 既に読み込み済みのテクスチャか検索
    for (uint32_t i = 0; i < textures_.size(); ++i) {
        if (textures_[i].filePath == filePath) {
            // 読み込み済みならそのハンドル(インデックス)を返す
            return i;
        }
    }

    // これから生成するテクスチャのインデックス（ハンドル）
    // textures_配列の末尾に追加されるので、現在のサイズが新しいインデックスになる
    const uint32_t handle = static_cast<uint32_t>(textures_.size());
    textures_.resize(handle + 1);
    textures_[handle].filePath = filePath;

    // 1. テクスチャファイルを読み込む
    DirectX::ScratchImage mipImages;
    bool isFallbackWhite = (filePath == "white" || filePath == "resources/white1x1.png" || filePath == "white1x1.png" || filePath.empty());
    if (!isFallbackWhite && !std::filesystem::exists(filePath)) {
        OutputDebugStringA(("[WARNING] TextureManager::Load: Texture file not found: " + filePath + ", fallback to white 1x1.\n").c_str());
        isFallbackWhite = true;
    }

    if (isFallbackWhite) {
        HRESULT hr = mipImages.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, 1, 1);
        assert(SUCCEEDED(hr));
        uint8_t* pixels = mipImages.GetPixels();
        pixels[0] = 255; // R
        pixels[1] = 255; // G
        pixels[2] = 255; // B
        pixels[3] = 255; // A
    } else {
        try {
            mipImages = LoadTexture(filePath);
        } catch (const std::exception& e) {
            OutputDebugStringA(("[WARNING] TextureManager::Load failed for: " + filePath + ", fallback to white 1x1. Error: " + e.what() + "\n").c_str());
            HRESULT hr = mipImages.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, 1, 1);
            assert(SUCCEEDED(hr));
            uint8_t* pixels = mipImages.GetPixels();
            pixels[0] = 255; // R
            pixels[1] = 255; // G
            pixels[2] = 255; // B
            pixels[3] = 255; // A
        }
    }
    const DirectX::TexMetadata &metadata = mipImages.GetMetadata();

    // 2. GPU上にテクスチャリソースを作成 (device_はComPtrなので.Get()は不要)
    textures_[handle].resource = CreateTextureResource(device_, metadata);

    // 3. テクスチャデータをGPUにアップロード (引数から.Get()を削除)
    textures_[handle].intermediateResource = UploadTextureData(
        textures_[handle].resource, mipImages, device_, commandList);

    // 4. シェーダーリソースビュー(SRV)を作成
    SrvManager::GetInstance()->Allocate(&textures_[handle].srvHandleCPU, &textures_[handle].srvHandleGPU);

    // --- ここから資料の内容で SRV の設定を行う ---
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = metadata.format;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

    // ★資料の指示：CubeMapか判定して設定を分岐
    if (metadata.IsCubemap()) {
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
        srvDesc.TextureCube.MostDetailedMip = 0;
        srvDesc.TextureCube.MipLevels = UINT_MAX; // 全レベル使用
        srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
    } else {
        // 今まで通りの 2D テクスチャ設定
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = static_cast<UINT>(metadata.mipLevels);
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.PlaneSlice = 0;
        srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
    }

    // SRVの生成
    device_->CreateShaderResourceView(textures_[handle].resource.Get(), &srvDesc, textures_[handle].srvHandleCPU);

    return handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE TextureManager::GetGpuHandle(uint32_t textureHandle) const {
    assert(textureHandle < textures_.size());
    return textures_[textureHandle].srvHandleGPU;
}

const D3D12_RESOURCE_DESC TextureManager::GetResourceDesc(uint32_t textureHandle) const {
    // ハンドルが範囲内かチェック
    assert(textureHandle < textures_.size());

    // リソースの情報を取得して返す
    return textures_[textureHandle].resource->GetDesc();
}
