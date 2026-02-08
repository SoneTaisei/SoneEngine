#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <string>
#include <vector>
#include <map>
#include "../externals/DirectXTex/DirectXTex.h"
#include "Utility/UtilityFunctions.h"


class TextureManager {
public:
    // シングルトンインスタンスを取得
    static TextureManager *GetInstance();

    // 初期化処理
    void Initialize(Microsoft::WRL::ComPtr<ID3D12Device> device);

    // 終了処理
    void Finalize();

    // テクスチャ読み込み
    uint32_t Load(const std::string &filePath, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList);

    // SRVディスクリプタヒープのポインタを取得
    ID3D12DescriptorHeap *GetSrvDescriptorHeap() const { return srvDescriptorHeap_.Get(); }

    // 指定したハンドルのGPUディスクリプタハンドルを取得
    D3D12_GPU_DESCRIPTOR_HANDLE GetGpuHandle(uint32_t textureHandle) const;

    // テクスチャの総数を取得
    size_t GetTextureCount() const { return textures_.size(); }

    // 指定したハンドルのリソース情報（幅・高さなど）を取得
    const D3D12_RESOURCE_DESC GetResourceDesc(uint32_t textureHandle) const;

    void AllocateDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE *out_cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE *out_gpu_handle);

private:
    // コンストラクタ、デストラクタなどをprivateにする（シングルトン）
    TextureManager() = default;
    ~TextureManager() = default;
    TextureManager(const TextureManager &) = delete;
    const TextureManager &operator=(const TextureManager &) = delete;

    // テクスチャ1枚分の情報を持つ構造体
    struct TextureData {
        Microsoft::WRL::ComPtr<ID3D12Resource> resource;
        Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource; // アップロード時に使用
        D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCPU;
        D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU;
        std::string filePath;
    };

    // DirectXデバイス
    Microsoft::WRL::ComPtr<ID3D12Device> device_;

    // SRV用のディスクリプタヒープ
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap_;
    uint32_t descriptorSizeSRV_ = 0;

    // 読み込んだテクスチャデータ
    std::vector<TextureData> textures_;

    // 次にディスクリプタを書き込むSRVヒープのインデックス
    uint32_t nextSrvIndex_ = 0;
};