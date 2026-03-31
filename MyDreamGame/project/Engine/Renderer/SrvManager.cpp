#include "SrvManager.h"
#include <cassert>

SrvManager *SrvManager::GetInstance() {
    static SrvManager instance;
    return &instance;
}

void SrvManager::Initialize(Microsoft::WRL::ComPtr<ID3D12Device> device) {
    device_ = device;
    srvDescriptorHeap_ = CreateDescriptorHeap(device_, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, kMaxCount, true);
    descriptorSizeSRV_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    nextSrvIndex_ = 1;
}

void SrvManager::Finalize() {
    srvDescriptorHeap_.Reset();
    device_ = nullptr;
}

void SrvManager::Allocate(D3D12_CPU_DESCRIPTOR_HANDLE *out_cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE *out_gpu_handle) {
    assert(nextSrvIndex_ < kMaxCount); // ヒープの空き容量チェック

    uint32_t currentSrvIndex = nextSrvIndex_++;

    *out_cpu_handle = srvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();
    out_cpu_handle->ptr += (static_cast<SIZE_T>(descriptorSizeSRV_) * currentSrvIndex);

    *out_gpu_handle = srvDescriptorHeap_->GetGPUDescriptorHandleForHeapStart();
    out_gpu_handle->ptr += (static_cast<SIZE_T>(descriptorSizeSRV_) * currentSrvIndex);
}