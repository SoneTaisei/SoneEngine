#pragma once
#include "DirectXTex/d3dx12.h"
#include "DirectXTex/DirectXTex.h"
#include <d3d12.h>
#include <string>
#include <wrl.h>

/// <summary>
/// テクスチャファイルの読み込み、D3D12テクスチャリソースの作成、GPU転送を担うクラス
/// </summary>
class TextureLoader {
public:
    /// <summary>
    /// 画像ファイル（PNG, JPG, DDS）を読み込み、ミップマップ付きの画像データを生成する
    /// </summary>
    /// <param name="filePath">テクスチャファイルのパス</param>
    /// <returns>読み込まれた画像データ（ファイルが存在しない場合は1x1の白テクスチャを返す安全設計）</returns>
    static DirectX::ScratchImage LoadTexture(const std::string &filePath);

    /// <summary>
    /// メタデータを基にDirectX12のテクスチャリソース（ID3D12Resource）を生成する
    /// </summary>
    /// <param name="device">DirectX12デバイス</param>
    /// <param name="metadata">テクスチャのメタデータ（解像度・フォーマットなど）</param>
    /// <returns>生成されたテクスチャリソース</returns>
    static Microsoft::WRL::ComPtr<ID3D12Resource> CreateTextureResource(
        Microsoft::WRL::ComPtr<ID3D12Device> device,
        const DirectX::TexMetadata &metadata);

    /// <summary>
    /// 画像データを中間バッファを経由してGPUのテクスチャリソースに転送する
    /// </summary>
    /// <param name="texture">転送先テクスチャリソース</param>
    /// <param name="mipImages">転送する画像データ</param>
    /// <param name="device">DirectX12デバイス</param>
    /// <param name="commandList">コマンドリスト</param>
    /// <returns>転送完了まで保持しておく必要がある中間バッファリソース（戻り値破棄厳禁）</returns>
    [[nodiscard]]
    static Microsoft::WRL::ComPtr<ID3D12Resource> UploadTextureData(
        Microsoft::WRL::ComPtr<ID3D12Resource> texture,
        const DirectX::ScratchImage &mipImages,
        Microsoft::WRL::ComPtr<ID3D12Device> device,
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList);
};