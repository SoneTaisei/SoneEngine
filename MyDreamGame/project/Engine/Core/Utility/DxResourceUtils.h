#pragma once
#include "Core/Utility/Structs.h"
#include <cstdint>
#include <d3d12.h>
#include <wrl.h>

class DxResourceUtils {
public:
    /// <summary>
    /// 定数バッファや頂点バッファ用のリソースを生成
    /// </summary>
    /// <param name="device">DirectX12デバイス</param>
    /// <pram name="sizeInBytes">生成するリソースのサイズ（バイト単位）</param>
    /// <returns>生成されたリソース（ID3D12Resource）</returns>
    static Microsoft::WRL::ComPtr<ID3D12Resource> CreateBufferResource(
        Microsoft::WRL::ComPtr<ID3D12Device> device, size_t sizeInBytes);

    /// <summary>
    /// DescriptorHeapを生成する
    /// </summary>
    /// <param name="device">DirectX12デバイス</param>
    /// <param name="heapType">ヒープの種類(RTV,DSV,CBV_SRV_UAV など)</param>
    /// <param name="numDescriptors">ヒープに含めるディスクリプタの数</param>
    /// <param name="shaderVisible">シェーダーから参照するかどうか</param>
    /// <returns>生成されたDescriptorHeap</returns>
    static Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(
        Microsoft::WRL::ComPtr<ID3D12Device> device, D3D12_DESCRIPTOR_HEAP_TYPE heapType,
        UINT numDescriptors, bool shaderVisible);

    /// <summary>
    /// 深度ステンシル用のテクスチャリソースを生成する
    /// </summary>
    /// <param name="device">D3D12デバイス</param>
    /// <param name="width">横幅解像度</param>
    /// <param name="height">縦幅解像度</param>
    /// <returns>生成された深度テクスチャリソース</returns>
    static Microsoft::WRL::ComPtr<ID3D12Resource> CreateDepthStencilTextureResource(
        Microsoft::WRL::ComPtr<ID3D12Device> device,
        int32_t width,
        int32_t height);

    /// <summary>
    /// オフスクリーン描画用のレンダーターゲットテクスチャリソースを生成する
    /// </summary>
    /// <param name="device">DirectX12デバイス</param>
    /// <param name="width">テクスチャの幅</param>
    /// <param name="height">テクスチャの高さ</param>
    /// <param name="format">テクスチャのピクセルフォーマット</param>
    /// <param name="clearColor">レンダーターゲットのクリアカラー(RGBA)</param>
    /// <param name="initialState">生成時のリソースステート</param>
    /// <returns>生成されたレンダーターゲットテクスチャリソース</returns>
    static Microsoft::WRL::ComPtr<ID3D12Resource> CreateRenderTextureResource(
        Microsoft::WRL::ComPtr<ID3D12Device> device, uint32_t width, uint32_t height,
        DXGI_FORMAT format, const Vector4 &clearColor, D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_RENDER_TARGET);

    /// <summary>
    /// デスクリプターヒープから指定したインデックスのCPUハンドルを取得する
    /// </summary>
    static D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap, uint32_t descriptorSize, uint32_t index);

    /// <summary>
    /// デスクリプタヒープから指定インデックスの GPU デスクリプタハンドルを取得する
    /// </summary>
    static D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap,
        uint32_t descriptorSize,
        uint32_t index);

};
