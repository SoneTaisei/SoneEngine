#pragma once
#include"Utility/Structs.h"
#include"GameObject/ResourceObject.h"

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

void Log(const std::string &message);

std::wstring ConvertString(const std::string &str);

std::string ConvertString(const std::wstring &str);

LONG WINAPI ExportDump(EXCEPTION_POINTERS *exception);

IDxcBlob *CompileShader(
	// CompilerするShaderファイルへのパス
	const std::wstring &filePath,
	// Compilerに使用するProfile
	const wchar_t *profile,
	// 初期化で生成したものを3つ
	IDxcUtils *dxcUtils,
	IDxcCompiler3 *dxcCompiler,
	IDxcIncludeHandler *includeHandler);

Microsoft::WRL::ComPtr<ID3D12Resource> CreateBufferResource(Microsoft::WRL::ComPtr<ID3D12Device> device, size_t sizeInBytes);

// DescriptorHeapの作成関数
Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(
	Microsoft::WRL::ComPtr<ID3D12Device> device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible);

// Textureデータを読む
DirectX::ScratchImage LoadTexture(const std::string &filePath);

// DirectX12のTextureResourceを作る
Microsoft::WRL::ComPtr<ID3D12Resource> CreateTextureResource(Microsoft::WRL::ComPtr<ID3D12Device> device, const DirectX::TexMetadata &metadata);

// 戻り値を破損してはならないのでこれを付ける
[[nodiscard]]
// TextureResouorceにデータを転送する
Microsoft::WRL::ComPtr<ID3D12Resource> UploadTextureData(Microsoft::WRL::ComPtr<ID3D12Resource> texture, const DirectX::ScratchImage &mipImages, Microsoft::WRL::ComPtr<ID3D12Device> device, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList);

Microsoft::WRL::ComPtr<ID3D12Resource> CreateDepthStencilTextureResource(Microsoft::WRL::ComPtr<ID3D12Device> device, int32_t width, int32_t height);

// DescriptorHandleを取得する(CPU)
D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap, uint32_t descriptorSize, uint32_t index);

// DescriptorHandleを取得する(GPU)
D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap, uint32_t descriptorSize, uint32_t index);

void CreateSphereMesh(std::vector<VertexData> &vertices, std::vector<uint32_t> &indices, float radius, int latDiv, int lonDiv);

/// <summary>
/// objファイルを読む関数 
/// </summary>
/// <param name="directoryPath"></param>
/// <param name="filename"></param>
/// <returns></returns>
ModelData LoadModelFile(const std::string &directoryPath, const std::string &filename);

MaterialData LoadMaterialTemplateFile(const std::string &directoryPath, const std::string &filename);

SoundData SoundLoadWave(const char *filename);

void SoundUnload(SoundData *soundData);

/*キー入力の取得
*********************************************************/

// 押されている時
bool IsKeyHeld(BYTE keys);

// キーが離された瞬間
bool IsKeyReleased(BYTE keys, BYTE preKeys);

// キーが押された瞬間
bool IsKeyPressed(BYTE keys, BYTE preKeys);

// 押されていないとき
bool IsKeyUp(BYTE keys);

SoundData SoundLoadMediaFoundation(const char *filename);

