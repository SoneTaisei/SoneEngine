#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <vector>
#include <list>
#include "Utility/Utilityfunctions.h"

// 前方宣言
class Sprite;
class DirectXCommon;

class SpriteCommon {
public:
    // 初期化 (デバイスとウィンドウサイズを受け取る)
    void Initialize(DirectXCommon *dxCommon, int windowWidth, int windowHeight);

    // 終了処理
    void Finalize();

    // 描画前処理 (共通の設定をコマンドリストに積む)
    void PreDraw(ID3D12GraphicsCommandList *commandList);

    // ★登録されている全スプライトを描画する
    void DrawAll();

    // リスト管理用 (Spriteクラスから呼ばれる)
    void AddSprite(Sprite *sprite);
    void RemoveSprite(Sprite *sprite);

    // ゲッター
    ID3D12Device *GetDevice() const { return device_; }
    ID3D12GraphicsCommandList *GetCommandList() const { return commandList_; }
    const Matrix4x4 &GetProjectionMatrix() const { return projectionMatrix_; }

    // ★追加: ビュー行列のゲッター
    const Matrix4x4 &GetViewMatrix() const { return viewMatrix_; }

    // ★追加: ビュー行列のセッター (メインループからカメラの行列を渡す用)
    void SetViewMatrix(const Matrix4x4 &matrix) { viewMatrix_ = matrix; }

private:
    // 共通リソース作成関数
    void CreateCommonResources();

    void CreateGraphicsPipeline();

private:
    DirectXCommon *dxCommon_ = nullptr;

    ID3D12Device *device_ = nullptr;
    ID3D12GraphicsCommandList *commandList_ = nullptr;

    // 共通の頂点・インデックスバッファ
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
    Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_;
    D3D12_INDEX_BUFFER_VIEW indexBufferView_{};
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;

    // 射影行列 (画面サイズ依存)
    Matrix4x4 projectionMatrix_{};

    // ★全スプライトのリスト
    std::list<Sprite *> sprites_;

    // ★追加: ビュー行列を保持する変数 (初期値は単位行列にしておく)
    Matrix4x4 viewMatrix_ = TransformFunctions::MakeIdentity4x4();
};

