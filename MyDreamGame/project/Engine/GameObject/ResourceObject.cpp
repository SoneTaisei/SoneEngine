#include "ResourceObject.h"

ResourceObject::ResourceObject(Microsoft::WRL::ComPtr<ID3D12Resource> resource)
	:resource_(resource) {
}

ID3D12Resource *ResourceObject::Get() const {
	return resource_.Get();
}
