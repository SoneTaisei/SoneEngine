#pragma once
#include "Core/Utility/UtilityFunctions.h"
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
    void Allocate(D3D12_CPU_DESCRIPTOR_HANDLE *out_cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE *out_gpu_handle, const char* caller = "Unknown");

    // 不要になったディスクリプタのハンドルを返却・再利用する関数
    void Free(D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle);

    uint32_t GetAllocatedCount() const { return nextSrvIndex_; }
    uint32_t GetMaxCount() const { return kMaxCount; }

private:
    SrvManager() = default;
    ~SrvManager() = default;
    SrvManager(const SrvManager &) = delete;
    const SrvManager &operator=(const SrvManager &) = delete;

    Microsoft::WRL::ComPtr<ID3D12Device> device_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap_;
    uint32_t descriptorSizeSRV_ = 0;
    uint32_t nextSrvIndex_ = 1; // 元のTextureManagerに合わせて1から
    const uint32_t kMaxCount = 65536; // 2048から65536に大幅拡張
    std::vector<uint32_t> freeList_;
};