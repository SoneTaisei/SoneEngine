#include "ViewProjection.h"
#include <cassert>

void ViewProjection::Initialize(ID3D12Device *device) {
    // 256バイトアライメントでリソース作成
    UINT size = (sizeof(ViewProjectionData) + 255) & ~255;
    resource_ = CreateBufferResource(device, size);

    // ★ ここで Map を行う！
    resource_->Map(0, nullptr, reinterpret_cast<void **>(&mappedData_));

    // 初期行列の設定
    mappedData_->viewProjectionMatrix = TransformFunctions::MakeIdentity4x4();
}

void ViewProjection::UpdateMatrix(const Matrix4x4 &view, const Matrix4x4 &projection) {
    // 行列を合成して転送
    mappedData_->viewProjectionMatrix = TransformFunctions::Multiply(view, projection);
}