#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <vector>
#include <string>
#include <list>
#include "Utility/Utilityfunctions.h"
#include "Utility/BlendMode.h"
<<<<<<< Updated upstream
=======

class Particle;
>>>>>>> Stashed changes

// 共通の頂点データ構造体
struct ParticleVertexData {
    Vector4 position;
    Vector2 texcoord;
    Vector3 normal;
};

class ParticleCommon {
public:
    void Initialize(ID3D12Device *device);
    void PreDraw(ID3D12GraphicsCommandList *commandList);

<<<<<<< Updated upstream
    // ブレンドモードを指定してPSOをセットする関数
=======
    // 登録されている全パーティクルを描画する
    void DrawAll();

    // リスト管理用
    void AddParticle(Particle *particle);
    void RemoveParticle(Particle *particle);

    // ブレンドモード切り替え関数 (BlendMode型を受け取る)
>>>>>>> Stashed changes
    void SetBlendMode(BlendMode blendMode);

    // ゲッター
    ID3D12Device *GetDevice() const { return device_; }
    ID3D12GraphicsCommandList *GetCommandList() const { return commandList_; }
    const D3D12_VERTEX_BUFFER_VIEW &GetVertexBufferView() const { return vertexBufferView_; }
    const Microsoft::WRL::ComPtr<ID3D12RootSignature> &GetRootSignature() const { return rootSignature_; }
    UINT GetVertexCount() const { return static_cast<UINT>(vertices_.size()); }

private:
    void CreateRootSignature();
    void CreatePipelineState();
    void CreateMesh(); // 共通の板ポリゴン生成

private:
    ID3D12Device *device_ = nullptr;
    ID3D12GraphicsCommandList *commandList_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
<<<<<<< Updated upstream

    // PSOを配列で管理 (kCountOfBlendMode は BlendMode.h で定義されている数)
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineStates_[kCountOfBlnedMode];
=======
    // すべてのブレンドモード用のPSOを配列で持つ
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineStates_[kCountOfBlnedMode];

    // 全パーティクルのリスト
    std::list<Particle *> particles_;
>>>>>>> Stashed changes

    // 共通の板ポリゴンデータ
    std::vector<ParticleVertexData> vertices_;
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
};