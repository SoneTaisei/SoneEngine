#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <vector>
#include <list>
#include "Core/Utility/Utilityfunctions.h"

// 前方宣言
class Sprite;
class DirectXCommon;

class SpriteCommon {
public:
    // 初期化 (デバイスとウィンドウサイズを受け取る)
    void Initialize(DirectXCommon *dxCommon, int windowWidth, int windowHeight);

    // 解像度変更時の更新
    void SetResolution(int windowWidth, int windowHeight);

    // 終了処理
    void Finalize();

    // 描画前処理 (共通の設定をコマンドリストに積む)
    void PreDraw();

    // ★登録されている全スプライトを描画する
    void DrawAll();

    // リスト管理用 (Spriteクラスから呼ばれる)
    void AddSprite(Sprite *sprite);
    void RemoveSprite(Sprite *sprite);
    void ClearAll() { sprites_.clear(); }

    // ゲッター
    ID3D12Device *GetDevice() const { return device_; }
    ID3D12GraphicsCommandList *GetCommandList() const { return commandList_; }
    const Matrix4x4 &GetProjectionMatrix() const { return projectionMatrix_; }

    // ビュー行列のゲッター
    const Matrix4x4 &GetViewMatrix() const { return viewMatrix_; }

    // ビュー行列のセッター (メインループからカメラの行列を渡す用)
    void SetViewMatrix(const Matrix4x4 &matrix) { viewMatrix_ = matrix; }

    // インデックス数のゲッター
    uint32_t GetIndexCount() const { return indexCount_; }

    // 基準解像度のセッター・ゲッター (デフォルト: 1280x720)
    void SetBaseResolution(float width, float height);
    float GetBaseWidth() const { return baseWidth_; }
    float GetBaseHeight() const { return baseHeight_; }

private:
    // 基準解像度と現在のウィンドウ解像度に基づいて正射影行列を更新する
    void UpdateProjectionMatrix();

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

    // 仮想基準解像度 (デフォルト: 1280x720)
    float baseWidth_ = 1280.0f;
    float baseHeight_ = 720.0f;

    // 現在のウィンドウ/描画ターゲット解像度
    int windowWidth_ = 1280;
    int windowHeight_ = 720;

    // ★全スプライトのリスト
    std::list<Sprite *> sprites_;

    // ★追加: ビュー行列を保持する変数 (初期値は単位行列にしておく)
    Matrix4x4 viewMatrix_ = TransformFunctions::MakeIdentity4x4();

    // ★これを追加: インデックス数を変数として保持する
    uint32_t indexCount_ = 6;
};

