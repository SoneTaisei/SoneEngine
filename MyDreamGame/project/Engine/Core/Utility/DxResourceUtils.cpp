#include "DxResourceUtils.h"
#include <cassert>
#include <stdexcept>

// ===================================================================
// バッファリソース（定数バッファ・頂点バッファ用アップロードヒープ）の生成
// ===================================================================
Microsoft::WRL::ComPtr<ID3D12Resource> DxResourceUtils::CreateBufferResource(
    Microsoft::WRL::ComPtr<ID3D12Device> device,
    size_t sizeInBytes) {

    assert(device != nullptr);

    // CPUから書き込めるアップロードヒープの設定
    D3D12_HEAP_PROPERTIES heapProperties{};
    heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

    // バッファリソースの設定
    D3D12_RESOURCE_DESC resourceDesc{};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDesc.Width = sizeInBytes;
    resourceDesc.Height = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    Microsoft::WRL::ComPtr<ID3D12Resource> resource = nullptr;
    HRESULT hr = device->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, // 初回ステート
        nullptr,
        IID_PPV_ARGS(&resource));

    if (FAILED(hr)) {
        throw std::runtime_error("DxResourceUtils::CreateBufferResource failed! VRAM might be full or arguments invalid.");
    }
    return resource;
}

// ===================================================================
// デスクリプタヒープの生成
// ===================================================================
Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> DxResourceUtils::CreateDescriptorHeap(
    Microsoft::WRL::ComPtr<ID3D12Device> device,
    D3D12_DESCRIPTOR_HEAP_TYPE heapType,
    UINT numDescriptors,
    bool shaderVisible) {

    assert(device != nullptr);

    D3D12_DESCRIPTOR_HEAP_DESC desc{};
    desc.Type = heapType;
    desc.NumDescriptors = numDescriptors;
    desc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> heap = nullptr;
    HRESULT hr = device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap));
    assert(SUCCEEDED(hr));
    return heap;
}

// ===================================================================
// 深度ステンシルテクスチャリソースの生成
// ===================================================================
Microsoft::WRL::ComPtr<ID3D12Resource> DxResourceUtils::CreateDepthStencilTextureResource(
    Microsoft::WRL::ComPtr<ID3D12Device> device,
    int32_t width,
    int32_t height) {

    assert(device != nullptr);

    D3D12_RESOURCE_DESC resourceDesc{};
    resourceDesc.Width = width;
    resourceDesc.Height = height;
    resourceDesc.MipLevels = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_HEAP_PROPERTIES heapProperties{};
    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT; // VRAM上に配置

    // 深度クリア値の設定
    D3D12_CLEAR_VALUE depthClearValue{};
    depthClearValue.DepthStencil.Depth = 1.0f;
    depthClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;

    Microsoft::WRL::ComPtr<ID3D12Resource> resource = nullptr;
    HRESULT hr = device->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &depthClearValue,
        IID_PPV_ARGS(&resource));
    assert(SUCCEEDED(hr));
    return resource;
}

// ===================================================================
// レンダーターゲットテクスチャリソースの生成
// ===================================================================
Microsoft::WRL::ComPtr<ID3D12Resource> DxResourceUtils::CreateRenderTextureResource(
    Microsoft::WRL::ComPtr<ID3D12Device> device,
    uint32_t width,
    uint32_t height,
    DXGI_FORMAT format,
    const Vector4 &clearColor,
    D3D12_RESOURCE_STATES initialState) {

    assert(device != nullptr);

    D3D12_RESOURCE_DESC resourceDesc{};
    resourceDesc.Width = width;
    resourceDesc.Height = height;
    resourceDesc.MipLevels = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.Format = format;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET; // レンダーターゲットフラグ

    D3D12_HEAP_PROPERTIES heapProperties{};
    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

    // クリアカラーの設定
    D3D12_CLEAR_VALUE clearValue{};
    clearValue.Format = format;
    clearValue.Color[0] = clearColor.x;
    clearValue.Color[1] = clearColor.y;
    clearValue.Color[2] = clearColor.z;
    clearValue.Color[3] = clearColor.w;

    Microsoft::WRL::ComPtr<ID3D12Resource> resource = nullptr;
    HRESULT hr = device->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        initialState,
        &clearValue,
        IID_PPV_ARGS(&resource));
    assert(SUCCEEDED(hr));
    return resource;
}

// ===================================================================
// CPU デスクリプタハンドルの取得
// ===================================================================
D3D12_CPU_DESCRIPTOR_HANDLE DxResourceUtils::GetCPUDescriptorHandle(
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap,
    uint32_t descriptorSize,
    uint32_t index) {

    D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
    handleCPU.ptr += (descriptorSize * index);
    return handleCPU;
}

// ===================================================================
// GPU デスクリプタハンドルの取得
// ===================================================================
D3D12_GPU_DESCRIPTOR_HANDLE DxResourceUtils::GetGPUDescriptorHandle(
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap,
    uint32_t descriptorSize,
    uint32_t index) {

    D3D12_GPU_DESCRIPTOR_HANDLE handleGPU = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
    handleGPU.ptr += (descriptorSize * index);
    return handleGPU;
}
