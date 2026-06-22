#pragma once
#include "Camera.h"

class MapEditorCamera : public Camera {
public:
    void Initialize(int kClientWidth, int kClientHeight) override;
    void Update(bool allowInput = true);
    void UpdateMatrix() override;
    
    // 現在のカメラのズーム倍率を取得（スクリーン座標からワールド座標への変換用）
    float GetZoom() const { return zoom_; }

private:
    float zoom_ = 32.0f; // 1マスのピクセルサイズに相当
    Vector3 initialTranslation_;
};
