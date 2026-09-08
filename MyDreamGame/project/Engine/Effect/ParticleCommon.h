#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <vector>
#include <string>
#include <list>
#include "Core/Utility/Utilityfunctions.h"
#include "Core/Utility/BlendMode.h"

#include <map>
#include <dxcapi.h>

class ParticleManager;

// 共通の頂点データ構造体
struct ParticleVertexData {
    Vector4 position;
    Vector2 texcoord;
    Vector3 normal;
};

// パイプラインキャッシュ用キー
struct ParticlePipelineKey {
    std::string vsPath;
    std::string psPath;
    BlendMode blendMode;

    bool operator<(const ParticlePipelineKey& other) const {
        if (vsPath != other.vsPath) return vsPath < other.vsPath;
        if (psPath != other.psPath) return psPath < other.psPath;
        return blendMode < other.blendMode;
    }
};

class ParticleCommon {
public:
    void Initialize(ID3D12Device *device);
    void PreDraw();

    void SetViewProjection(const Matrix4x4 &viewProjection) {
        viewProjection_ = viewProjection;
    }

    // 登録されている全パーティクルを描画する
    void DrawAll();

    // リスト管理用
    void AddParticle(ParticleManager *ParticleManager);
    void RemoveParticle(ParticleManager *ParticleManager);
    void ClearAll() { particles_.clear(); }

    // ブレンドモード切り替え関数 (BlendMode型を受け取る - 既存互換)
    void SetBlendMode(BlendMode blendMode);

    // シェーダー・ブレンドモード指定でPSOを設定する
    void SetPipelineState(const std::string& vsPath, const std::string& psPath, BlendMode blendMode);

    // キャッシュを破棄して再コンパイルを促す
    void ReloadShaders();

    // パイプラインステート取得（無ければ生成）
    ID3D12PipelineState* GetOrCreatePipelineState(const std::string& vsPath, const std::string& psPath, BlendMode blendMode);

    // ゲッター
    ID3D12Device *GetDevice() const { return device_; }
    ID3D12GraphicsCommandList *GetCommandList() const { return commandList_; }
    const D3D12_VERTEX_BUFFER_VIEW &GetVertexBufferView() const { return vertexBufferView_; }
    const Microsoft::WRL::ComPtr<ID3D12RootSignature> &GetRootSignature() const { return rootSignature_; }
    UINT GetVertexCount() const { return static_cast<UINT>(vertices_.size()); }
    // カメラ行列をセットする関数
    void SetCamera(const Matrix4x4 &cameraMatrix) {
        cameraMatrix_ = cameraMatrix;
    }

    // 保存したカメラ行列を取得する関数 (ParticleManagerが使う)
    const Matrix4x4 &GetCameraMatrix() const {
        return cameraMatrix_;
    }

    // DrawAllの引数は viewProjection だけでOK
    void DrawAll(const Matrix4x4 &viewProjection);

private:
    void CreateRootSignature();
    void CreatePipelineState();
    void CreateMesh(); // 共通の板ポリゴン生成

    Microsoft::WRL::ComPtr<ID3DBlob> CompileShaderInternal(const std::string& filePath, const wchar_t* profile);
    Microsoft::WRL::ComPtr<ID3D12PipelineState> CreateSinglePipelineState(
        ID3DBlob* vsBlob, ID3DBlob* psBlob, BlendMode blendMode);

private:
    ID3D12Device *device_ = nullptr;
    ID3D12GraphicsCommandList *commandList_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;

    // DXCコンパイラインスタンス
    Microsoft::WRL::ComPtr<IDxcUtils> dxcUtils_;
    Microsoft::WRL::ComPtr<IDxcCompiler3> dxcCompiler_;
    Microsoft::WRL::ComPtr<IDxcIncludeHandler> includeHandler_;

    // PSOキャッシュ
    std::map<ParticlePipelineKey, Microsoft::WRL::ComPtr<ID3D12PipelineState>> psoCache_;
    std::map<std::string, Microsoft::WRL::ComPtr<ID3DBlob>> shaderBlobCache_;

    // デフォルトPSOを配列で管理 (kCountOfBlendMode は BlendMode.h で定義されている数)
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineStates_[kCountOfBlendMode];

    // 全パーティクルのリスト
    std::list<ParticleManager *> particles_;

    // バッファにデータを書き込むためのポインタ
    Matrix4x4 viewProjection_;

    // ViewProjection用リソース
    Microsoft::WRL::ComPtr<ID3D12Resource> wvpResource_;
    Matrix4x4 *wvpData_ = nullptr;

    // 共通の板ポリゴンデータ
    std::vector<ParticleVertexData> vertices_;
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};

    // カメラ行列保存用
    Matrix4x4 cameraMatrix_ = TransformFunctions::MakeIdentity4x4();
};