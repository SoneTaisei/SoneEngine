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

void SrvManager::Allocate(D3D12_CPU_DESCRIPTOR_HANDLE *out_cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE *out_gpu_handle, const char* caller) {
    uint32_t currentSrvIndex = 0;

    if (!freeList_.empty()) {
        currentSrvIndex = freeList_.back();
        freeList_.pop_back();
    } else {
        assert(nextSrvIndex_ < kMaxCount); // ヒープの空き容量チェック
        currentSrvIndex = nextSrvIndex_++;
    }

    // 200件ごと、または高負荷時にログ出力
    if (currentSrvIndex > 0 && currentSrvIndex % 200 == 0) {
        char logBuf[256];
        snprintf(logBuf, sizeof(logBuf), "[SrvManager] Alloc #%u / %u (Caller: %s, FreeList: %zu)\n", currentSrvIndex, kMaxCount, caller ? caller : "Unknown", freeList_.size());
        OutputDebugStringA(logBuf);
    }

    *out_cpu_handle = srvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();
    out_cpu_handle->ptr += (static_cast<SIZE_T>(descriptorSizeSRV_) * currentSrvIndex);

    *out_gpu_handle = srvDescriptorHeap_->GetGPUDescriptorHandleForHeapStart();
    out_gpu_handle->ptr += (static_cast<SIZE_T>(descriptorSizeSRV_) * currentSrvIndex);
}

void SrvManager::Free(D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle) {
    (void)gpu_handle;
    if (cpu_handle.ptr == 0 || descriptorSizeSRV_ == 0 || !srvDescriptorHeap_) return;

    SIZE_T heapStart = srvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart().ptr;
    if (cpu_handle.ptr < heapStart) return;

    SIZE_T offset = cpu_handle.ptr - heapStart;
    if (offset % descriptorSizeSRV_ != 0) return;

    uint32_t index = static_cast<uint32_t>(offset / descriptorSizeSRV_);
    if (index >= 1 && index < kMaxCount) {
        freeList_.push_back(index);
    }
}