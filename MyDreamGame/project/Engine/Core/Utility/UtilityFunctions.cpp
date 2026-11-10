#include "Core/Utility/LogManager.h"
#pragma warning(disable: 4828)
#include "UtilityFunctions.h"
#include <fstream>
#include <mutex>
#include <filesystem>

void Log(const std::string &message) {
	// デバッグ出力（従来の動作）
	OutputDebugStringA(message.c_str());

	// ログファイルを一度だけ作成して使います（スレッドセーフ）
	static std::once_flag s_logInitFlag;
	static std::ofstream s_logStream;
	static std::mutex s_logMutex;

	std::call_once(s_logInitFlag, []() {
		try {
			// logs ディレクトリを作成
			std::filesystem::create_directories("logs");

			// 現在の時刻を秒単位に丸める
			auto now = std::chrono::system_clock::now();
			auto nowSeconds = std::chrono::time_point_cast<std::chrono::seconds>(now);

			// ローカルタイムゾーンに変換してフォーマット（文字コードと同じ書式を使用）
			std::chrono::zoned_time localTime{ std::chrono::current_zone(), nowSeconds };
			std::string dateString = std::format("{:%Y%d_%H%M%S}", localTime);

			// ファイルパスを作成して open（追記モード）
			std::string logFilePath = std::string("logs/") + dateString + ".log";
			s_logStream.open(logFilePath, std::ios::app | std::ios::binary);
		} catch(...) {
			// 例外は無視してデバッグ出力のみ行う（ログ失敗してもアプリが止まらないようにする）
		}
				   });

				   // 実際の書き込み
	std::lock_guard<std::mutex> lock(s_logMutex);
	if(s_logStream && s_logStream.good()) {
		s_logStream << message;
		s_logStream.flush();
	}
}

std::wstring ConvertString(const std::string &str) {
    return StringUtils::ConvertString(str);
}
std::string ConvertString(const std::wstring &str) {
    return StringUtils::ConvertString(str);
}

IDxcBlob *CompileShader(
    const std::wstring &filePath,
    const wchar_t *profile,
    IDxcUtils *dxcUtils,
    IDxcCompiler3 *dxcCompiler,
    IDxcIncludeHandler *includeHandler) {
    return ShaderCompiler::CompileShader(filePath, profile, dxcUtils, dxcCompiler, includeHandler);
}

// --- CreateBufferResource ---
Microsoft::WRL::ComPtr<ID3D12Resource> CreateBufferResource(Microsoft::WRL::ComPtr<ID3D12Device> device, size_t sizeInBytes) {
    return DxResourceUtils::CreateBufferResource(device, sizeInBytes);
}

// --- CreateDescriptorHeap ---
Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(Microsoft::WRL::ComPtr<ID3D12Device> device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible) {
    return DxResourceUtils::CreateDescriptorHeap(device, heapType, numDescriptors, shaderVisible);
}

// Textureデータを読む
// --- LoadTexture ---
DirectX::ScratchImage LoadTexture(const std::string &filePath) {
    return TextureLoader::LoadTexture(filePath);
}

// --- CreateTextureResource ---
Microsoft::WRL::ComPtr<ID3D12Resource> CreateTextureResource(Microsoft::WRL::ComPtr<ID3D12Device> device, const DirectX::TexMetadata &metadata) {
    return TextureLoader::CreateTextureResource(device, metadata);
}

// --- UploadTextureData ---
Microsoft::WRL::ComPtr<ID3D12Resource> UploadTextureData(Microsoft::WRL::ComPtr<ID3D12Resource> texture, const DirectX::ScratchImage &mipImages, Microsoft::WRL::ComPtr<ID3D12Device> device, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList) {
    return TextureLoader::UploadTextureData(texture, mipImages, device, commandList);
}

// --- CreateDepthStencilTextureResource ---
Microsoft::WRL::ComPtr<ID3D12Resource> CreateDepthStencilTextureResource(Microsoft::WRL::ComPtr<ID3D12Device> device, int32_t width, int32_t height) {
    return DxResourceUtils::CreateDepthStencilTextureResource(device, width, height);
}

// --- GetCPUDescriptorHandle ---
D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap, uint32_t descriptorSize, uint32_t index) {
    return DxResourceUtils::GetCPUDescriptorHandle(descriptorHeap, descriptorSize, index);
}

// --- GetGPUDescriptorHandle ---
D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap, uint32_t descriptorSize, uint32_t index) {
    return DxResourceUtils::GetGPUDescriptorHandle(descriptorHeap, descriptorSize, index);
}

void CreateSphereMesh(std::vector<VertexData> &vertices, std::vector<uint32_t> &indices, float radius, int latDiv, int lonDiv) {
    MeshGenerator::CreateSphereMesh(vertices, indices, radius, latDiv, lonDiv);
}

void CreateBoxMesh(std::vector<SkyboxVertexData> &vertices, std::vector<uint32_t> &indices) {
    MeshGenerator::CreateBoxMesh(vertices, indices);
}

ModelData LoadModelFile(const std::string &directoryPath, const std::string &filename) {
    return ModelLoader::LoadModelFile(directoryPath, filename);
}

// --- CreateRenderTextureResource ---
Microsoft::WRL::ComPtr<ID3D12Resource> CreateRenderTextureResource(Microsoft::WRL::ComPtr<ID3D12Device> device, uint32_t width, uint32_t height, DXGI_FORMAT format, const Vector4 &clearColor, D3D12_RESOURCE_STATES initialState) {
    return DxResourceUtils::CreateRenderTextureResource(device, width, height, format, clearColor, initialState);
}

