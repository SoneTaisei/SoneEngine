#pragma once
#include"Core/Utility/Structs.h"
class ResourceObject {
public:
	ResourceObject(Microsoft::WRL::ComPtr<ID3D12Resource> resource);
	// デストラクタはオブジェクトの寿命が尽きた時に呼ばれる
	~ResourceObject() = default;

	ID3D12Resource *Get() const;

private:
	Microsoft::WRL::ComPtr<ID3D12Resource> resource_;
};

