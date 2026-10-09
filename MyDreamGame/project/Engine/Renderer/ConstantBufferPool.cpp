#include "ConstantBufferPool.h"
#include "Core/Utility/UtilityFunctions.h"

ConstantBufferPool* ConstantBufferPool::GetInstance() {
    static ConstantBufferPool instance;
    return &instance;
}

ConstantBufferPool::Allocation ConstantBufferPool::Allocate(ID3D12Device* device, uint32_t sizeInBytes) {
    Allocation allocation;
    if (!device || sizeInBytes == 0 || sizeInBytes > kPageSize) {
        return allocation;
    }

    const uint32_t size = (sizeInBytes + kAlignment - 1) & ~(kAlignment - 1);

    // 使い終わった区画があればそれを配る
    auto& freeList = freeLists_[size];
    if (!freeList.empty()) {
        allocation = freeList.back();
        freeList.pop_back();
        return allocation;
    }

    // 今のページに空きがあれば先頭から切り出す
    for (int i = static_cast<int>(pages_.size()) - 1; i >= 0; --i) {
        Page& page = pages_[i];
        if (page.capacity - page.used >= size) {
            allocation.cpu = page.cpu + page.used;
            allocation.gpuAddress = page.gpuAddress + page.used;
            allocation.size = size;
            allocation.pageIndex = i;
            allocation.offset = page.used;
            allocation.generation = generation_;
            page.used += size;
            return allocation;
        }
    }

    // 足りなければページを増やす
    Page page;
    page.resource = CreateBufferResource(device, kPageSize);
    if (!page.resource) {
        return allocation;
    }
    page.resource->Map(0, nullptr, reinterpret_cast<void**>(&page.cpu));
    page.gpuAddress = page.resource->GetGPUVirtualAddress();
    page.capacity = kPageSize;
    page.used = size;

    allocation.cpu = page.cpu;
    allocation.gpuAddress = page.gpuAddress;
    allocation.size = size;
    allocation.pageIndex = static_cast<int>(pages_.size());
    allocation.offset = 0;
    allocation.generation = generation_;

    pages_.push_back(std::move(page));
    return allocation;
}

void ConstantBufferPool::Free(Allocation& allocation) {
    if (!allocation.IsValid()) return;

    // Shutdown 後（ページが無い）に返ってきたぶんは、参照先がもう無いので捨てるだけ
    if (allocation.generation == generation_ &&
        allocation.pageIndex >= 0 && allocation.pageIndex < static_cast<int>(pages_.size())) {
        freeLists_[allocation.size].push_back(allocation);
    }
    allocation = Allocation{};
}

void ConstantBufferPool::Shutdown() {
    freeLists_.clear();
    for (auto& page : pages_) {
        if (page.resource && page.cpu) {
            page.resource->Unmap(0, nullptr);
            page.cpu = nullptr;
        }
    }
    pages_.clear();
    ++generation_;
}
