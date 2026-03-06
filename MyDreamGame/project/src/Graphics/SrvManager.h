#pragma once
#include "Utility/UtilityFunctions.h"
#include <cstdint>
#include <d3d12.h>
#include <wrl.h>

class SrvManager {
public:
    static SrvManager *GetInstance();

    void Initialize(Microsoft::WRL::ComPtr<ID3D12Device> device);
    void Finalize();

    ID3D12DescriptorHeap *GetSrvDescriptorHeap() const { return srvDescriptorHeap_.Get(); }

    // 空いているディスクリプタのハンドルを割り当てる関数
    void Allocate(D3D12_CPU_DESCRIPTOR_HANDLE *out_cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE *out_gpu_handle);

private:
    SrvManager() = default;
    ~SrvManager() = default;
    SrvManager(const SrvManager &) = delete;
    const SrvManager &operator=(const SrvManager &) = delete;

    Microsoft::WRL::ComPtr<ID3D12Device> device_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap_;
    uint32_t descriptorSizeSRV_ = 0;
    uint32_t nextSrvIndex_ = 1; // 元のTextureManagerに合わせて1から
    const uint32_t kMaxCount = 2048;
};