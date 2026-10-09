#pragma once
#include <cstdint>
#include <unordered_map>
#include <vector>
#include <wrl/client.h>
#include <d3d12.h>

// 定数バッファ用のちいさな置き場（サブアロケータ）
//
// ブロック 1 個ごとに CreateCommittedResource で 256 バイトのバッファを作ると、
// マップを開くたびに数百〜数千回の生成が走って読み込みが目に見えて重くなる。
// そこで大きめのアップロードバッファ（ページ）をまとめて作り、そこから 256 バイト単位で
// 切り出して配る。解放されたぶんは空きリストに戻して使い回すので、
// マップの作り直しを何度繰り返してもページは増えない。
class ConstantBufferPool {
public:
    // 切り出した 1 区画。cpu へ書き込み、gpuAddress をそのまま定数バッファビューに渡す
    struct Allocation {
        uint8_t* cpu = nullptr;
        D3D12_GPU_VIRTUAL_ADDRESS gpuAddress = 0;
        uint32_t size = 0;   // 256 バイト単位に切り上げ済み
        int pageIndex = -1;
        uint32_t offset = 0;
        uint32_t generation = 0; // Shutdown をまたいだ区画を取り違えないための世代番号
        bool IsValid() const { return cpu != nullptr; }
    };

    static ConstantBufferPool* GetInstance();

    // 定数バッファ 1 個ぶんを確保する（256 バイト単位に切り上げられる）
    Allocation Allocate(ID3D12Device* device, uint32_t sizeInBytes);
    // 確保した区画を返す（空きリストに戻り、次の確保で使い回される）
    void Free(Allocation& allocation);
    // 全ページを解放する。デバイスを破棄する前に一度だけ呼ぶ
    void Shutdown();

private:
    ConstantBufferPool() = default;
    ~ConstantBufferPool() = default;
    ConstantBufferPool(const ConstantBufferPool&) = delete;
    ConstantBufferPool& operator=(const ConstantBufferPool&) = delete;

    struct Page {
        Microsoft::WRL::ComPtr<ID3D12Resource> resource;
        uint8_t* cpu = nullptr;
        D3D12_GPU_VIRTUAL_ADDRESS gpuAddress = 0;
        uint32_t capacity = 0;
        uint32_t used = 0;
    };

    static const uint32_t kAlignment = 256;
    static const uint32_t kPageSize = 1024 * 1024; // 1MB（256 バイト区画 4096 個ぶん）

    uint32_t generation_ = 1;
    std::vector<Page> pages_;
    // 区画の大きさごとの空きリスト（使い回し用）
    std::unordered_map<uint32_t, std::vector<Allocation>> freeLists_;
};
