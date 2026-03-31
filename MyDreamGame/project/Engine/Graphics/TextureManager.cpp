#include "TextureManager.h"
#include "Renderer/SrvManager.h"
#include <cassert>

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

uint32_t TextureManager::Load(const std::string &filePath, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList) {
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

    // 1. テクスチャファイルを読み込む (変更なし)
    DirectX::ScratchImage mipImages = LoadTexture(filePath);
    const DirectX::TexMetadata &metadata = mipImages.GetMetadata();

    // 2. GPU上にテクスチャリソースを作成 (device_はComPtrなので.Get()は不要)
    textures_[handle].resource = CreateTextureResource(device_, metadata);

    // 3. テクスチャデータをGPUにアップロード (引数から.Get()を削除)
    textures_[handle].intermediateResource = UploadTextureData(
        textures_[handle].resource, mipImages, device_, commandList);

    // 4. シェーダーリソースビュー(SRV)を作成
    SrvManager::GetInstance()->Allocate(&textures_[handle].srvHandleCPU, &textures_[handle].srvHandleGPU);

    // SRVの設定
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = metadata.format;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = static_cast<UINT>(metadata.mipLevels);

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
