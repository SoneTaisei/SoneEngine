#pragma once
#include "Primitive.h"
#include <map>
#include <memory>
#include <string>

class PrimitiveManager {
public:
    static PrimitiveManager* GetInstance() {
        static PrimitiveManager instance;
        return &instance;
    }

    void Initialize(ID3D12Device* device);

    // プリミティブを取得する。存在しない場合は生成する。
    Primitive* GetPrimitive(PrimitiveType type, float size = 1.0f, uint32_t segments = 16);

private:
    PrimitiveManager() = default;
    ~PrimitiveManager() = default;

    ID3D12Device* device_ = nullptr;
    // キーは形状タイプとパラメータを組み合わせたもの
    std::map<std::string, std::unique_ptr<Primitive>> primitiveRegistry_;
};
