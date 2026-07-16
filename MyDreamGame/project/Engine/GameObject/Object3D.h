#pragma once
#include "Core/Utility/Structs.h"
#include "Core/Utility/UtilityFunctions.h"
#include "Resource/Model/Model.h"
#include "Core/Utility/BlendMode.h"
#include <deque>

class Object3D {
    friend class Renderer;
public:
    void Initialize(ID3D12Device *device, Model *model);
    void Update();
    void Draw();
    void DisplayImGui(const std::string &label);

    static void SetEnvironmentMapHandle(D3D12_GPU_DESCRIPTOR_HANDLE handle) {
        sEnvironmentMapHandle = handle;
    }
    static D3D12_GPU_DESCRIPTOR_HANDLE GetEnvironmentMapHandle() {
        return sEnvironmentMapHandle;
    }

    // --- Transformのゲッター ---
    const Vector3 &GetTranslation() const { return transform_.translate; }
    const Vector3 &GetRotation() const { return transform_.rotate; }
    const Vector3 &GetScale() const { return transform_.scale; }

    // --- Transformのセッター ---
    void SetTranslation(const Vector3 &translate) { transform_.translate = translate; }
    void SetRotation(const Vector3 &rotate) { transform_.rotate = rotate; }
    void SetScale(const Vector3 &scale) { transform_.scale = scale; }
    
    void SetTextureHandle(D3D12_GPU_DESCRIPTOR_HANDLE handle) {
        if (model_) {
            model_->SetTextureHandle(handle);
        }
    }

    Material &GetMaterial() { return material_; }

    // --- 名前関連 ---
    const std::string &GetName() const { return name_; }
    void SetName(const std::string &name) { name_ = name; }
    void SetBlendMode(BlendMode blendMode) { blendMode_ = blendMode; }
    void SetIsDoubleSided(bool isDoubleSided) { isDoubleSided_ = isDoubleSided; }

private:
    std::string name_ = "GameObject"; // ヒエラルキー表示用の名前
    BlendMode blendMode_ = BlendMode::kBlendModeNormal;
    bool isDoubleSided_ = false;
    // マテリアル
    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
    Material *mappedMaterial_ = nullptr;

    // 平行光源 (Directional Light)
    Microsoft::WRL::ComPtr<ID3D12Resource> lightResource_;
    DirectionalLight *mappedLight_ = nullptr;

    // 座標変換 (World, WVP)
    Microsoft::WRL::ComPtr<ID3D12Resource> transformResource_;
    TransformMatrix *mappedTransform_ = nullptr;

    // ポイントライト (Point Light)
    Microsoft::WRL::ComPtr<ID3D12Resource> pointLightResource_;
    PointLight *mappedPointLight_ = nullptr; // 名前を統一感あるものに変更

    Model *model_ = nullptr;

    // CPU側データ
    EulerTransform transform_;
    Material material_;
    DirectionalLight light_;
    PointLight pointLight_; // CPU側でも値を保持しておくと便利

    static D3D12_GPU_DESCRIPTOR_HANDLE sEnvironmentMapHandle;

    // テクスチャ関連
    Microsoft::WRL::ComPtr<ID3D12Resource> textureResource_;
    uint32_t textureSrvHandle_ = 0;

    // 環境マップ用
    D3D12_GPU_DESCRIPTOR_HANDLE environmentMapSrvHandle_{};

    ModelData *modelData_ = nullptr;

    // --- 残像 (Trail) 設定 ---
    bool showTrail_ = false;
    int trailStep_ = 5;
    int trailLength_ = 600; // デフォルトで10秒分表示
    bool trailFadeOut_ = true; // 過去になるほど透明にするか
    BlendMode trailBlendMode_ = BlendMode::kBlendModeAdd;
    float trailStartAlpha_ = 0.5f;

    std::deque<EulerTransform> trailHistory_;
    static const int kMaxHistory = 10000; // 裏で記録しておく最大フレーム数

    static const int kMaxTrails = 1024;
    Microsoft::WRL::ComPtr<ID3D12Resource> trailTransformResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> trailMaterialResource_;
    uint8_t* mappedTrailTransform_ = nullptr;
    uint8_t* mappedTrailMaterial_ = nullptr;
};
