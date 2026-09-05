#include "Core/Utility/LogManager.h"
#pragma warning(disable: 4828)
#include "UtilityFunctions.h"
#include <map>
#include <fstream>
#include <mutex>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "Renderer/SrvManager.h"
#include <algorithm>
#include <filesystem>

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
#ifdef USE_IMGUI


	if(ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
		return true;
	}
#endif // USE_IMGUI

	// メッセージに応じてゲーム固有の処理を行う
	switch(msg) {
		// ウィンドウが破壊された
	case WM_DESTROY:
		// OSに応じて、アプリ固有の終了を伝える
		PostQuitMessage(0);
		return 0;
	}

	// 標準のメッセージ処理を行う
	return DefWindowProc(hwnd, msg, wparam, lparam);
}

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
	if(str.empty()) {
		return std::wstring();
	}

	auto sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char *>(&str[0]), static_cast<int>(str.size()), NULL, 0);
	if(sizeNeeded == 0) {
		return std::wstring();
	}
	std::wstring result(sizeNeeded, 0);
	MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char *>(&str[0]), static_cast<int>(str.size()), &result[0], sizeNeeded);
	return result;
}

std::string ConvertString(const std::wstring &str) {
	if(str.empty()) {
		return std::string();
	}

	auto sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), NULL, 0, NULL, NULL);
	if(sizeNeeded == 0) {
		return std::string();
	}
	std::string result(sizeNeeded, 0);
	WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), result.data(), sizeNeeded, NULL, NULL);
	return result;
}

std::string str0{ "STRING" };

std::string str1{ std::to_string(10) };

LONG WINAPI ExportDump(EXCEPTION_POINTERS *exception) {
	// 時刻を取得して、時刻を名前に入れたファイルを作成、dumpsディレクトリ以下に出力
	SYSTEMTIME time;
	GetLocalTime(&time);
	wchar_t filePath[MAX_PATH] = { 0 };

	// ディレクトリ作成（失敗しても続行）
	if(!CreateDirectoryW(L"./Dumps", nullptr)) {
		DWORD err = GetLastError();
		if(err != ERROR_ALREADY_EXISTS) {
			Log(std::format("CreateDirectory failed, err:{}\n", err));
		}
	}

	// ファイル名（秒単位）
	StringCchPrintfW(filePath, MAX_PATH, L"./Dumps/%04d-%02d-%02d_%02d%02d%02d.dmp",
					 time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);

				 // ログにパスを出力
	Log(std::format("ExportDump: target path: {}\n", ConvertString(filePath)));

	// ファイル作成
	HANDLE dumpFileHandle = CreateFileW(
		filePath,
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_WRITE | FILE_SHARE_READ,
		nullptr,
		CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL,
		nullptr);

	if(dumpFileHandle == INVALID_HANDLE_VALUE) {
		DWORD err = GetLastError();
		Log(std::format("CreateFileW failed, err:{}\n", err));
		return EXCEPTION_EXECUTE_HANDLER;
	}

	// processId(exeID)とクラッシュ(例外)の発生したthreadIDを取得
	DWORD processId = GetCurrentProcessId();
	DWORD threadId = GetCurrentThreadId();

	// 設定情報を出力
	MINIDUMP_EXCEPTION_INFORMATION minidumpInformation{};
	minidumpInformation.ThreadId = threadId;
	minidumpInformation.ExceptionPointers = exception;
	minidumpInformation.ClientPointers = TRUE;

	// Dumpを出力。結果をログに残す
	BOOL writeResult = MiniDumpWriteDump(
		GetCurrentProcess(),
		processId,
		dumpFileHandle,
		MiniDumpNormal,
		&minidumpInformation,
		nullptr,
		nullptr);

	if(!writeResult) {
		DWORD err = GetLastError();
		Log(std::format("MiniDumpWriteDump failed, err:{}\n", err));
	} else {
		Log(std::format("MiniDumpWriteDump succeeded: {}\n", ConvertString(filePath)));
	}

	CloseHandle(dumpFileHandle);

	// ほかに関連付けられているSEH例外ハンドラがあれば実行。通常プロセスを終了する。
	return EXCEPTION_EXECUTE_HANDLER;
}

IDxcBlob *CompileShader(
	// CompilerするShaderファイルへのパス
	const std::wstring &filePath,
	// Compilerに使用するProfile
	const wchar_t *profile,
	// 初期化で生成したものを渡す
	IDxcUtils *dxcUtils,
	IDxcCompiler3 *dxcCompiler,
	IDxcIncludeHandler *includeHandler) {

	/*********************************************************
	*1. hlsl繝輔ぃ繧､繝ｫ繧定ｪｭ繧
	*********************************************************/

	// これからシェーダーをコンパイルする旨をログに出す
	Log(ConvertString(std::format(L"Begin CompileShader, path:{},profile:{}\n", filePath, profile)));
	// hlsl繝輔ぃ繧､繝ｫ繧定ｪｭ繧
	IDxcBlobEncoding *shaderSource = nullptr;
	HRESULT hr = dxcUtils->LoadFile(filePath.c_str(), nullptr, &shaderSource);
	if (FAILED(hr)) {
		wchar_t buf[MAX_PATH];
		GetCurrentDirectoryW(MAX_PATH, buf);
		Log(ConvertString(std::format(L"Failed to load shader file: {}. hr: {:08X}, CWD: {}\n", filePath, hr, buf)));
	}
	// あきらめなかったら止める
	assert(SUCCEEDED(hr));
	// 読み込んだファイルの中身を設定する
	DxcBuffer shaderSourceBuffer;
	shaderSourceBuffer.Ptr = shaderSource->GetBufferPointer();
	shaderSourceBuffer.Size = shaderSource->GetBufferSize();
	shaderSourceBuffer.Encoding = DXC_CP_UTF8;// UTF8の文字コードであることを通知

	/*********************************************************
	2.Compileする
	*********************************************************/

	LPCWSTR arguments[] = {
        filePath.c_str(),         // コンパイル対象のhlslファイル名
        L"-E", L"main",           // エントリーポイントの指定。基本的にmain以外にはしない
        L"-T", profile,           // ShaderProfileの設定
        L"-Zi", L"-Qembed_debug", // デバッグ用の情報を埋め込む
        L"-Od",                   // 最適化を外しておく
        L"-Zpr",                  // メモリレイアウトは行優先
        L"-HV", L"2021",          // ※これを追加：HLSL2021ルールを適用してC++と同じ型名を使えるようにする
    };
	// 実際にShaderをコンパイルする
	IDxcResult *shaderResult = nullptr;
	hr = dxcCompiler->Compile(
		&shaderSourceBuffer,
		arguments,
		_countof(arguments),
		includeHandler,
		IID_PPV_ARGS(&shaderResult)
	);
	// コンパイラエラーではなくdxcが起動できないなどの致命的なエラー
	assert(SUCCEEDED(hr));
/*********************************************************
	3.警告やエラーが出ているか確認
	*********************************************************/

	IDxcBlobUtf8 *shaderError = nullptr;
	shaderResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&shaderError), nullptr);

	// shaderErrorが作られており、かつ中身の文字列の長さが0ではない場合だけエラーとみなす
	if(shaderError != nullptr && shaderError->GetStringLength() != 0) {
		Log(shaderError->GetStringPointer());
		// 警告やエラー絶対ダメ
		assert(false);
	}
	/*********************************************************
	4.Compile結果を受け取って返す
	*********************************************************/

	// コンパイル結果から実行用のバイナリ部分を取得
	IDxcBlob *shaderBlob = nullptr;
	hr = shaderResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
	assert(SUCCEEDED(hr));
	// 成功したログを出す
	Log(ConvertString(std::format(L"Compile Succeeded, path:{},profile:{}\n", filePath, profile)));
	// もう使わないソースを開放
	shaderSource->Release();
	shaderResult->Release();
	// 実行用のバイナリを返却
	return shaderBlob;
}

Microsoft::WRL::ComPtr<ID3D12Resource> CreateBufferResource(Microsoft::WRL::ComPtr<ID3D12Device> device, size_t sizeInBytes) {
	assert(device != nullptr); // 安全チェック

	// アップロード用のヒープの設定（CPUからGPUにデータを送る用）
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

	// バッファリソースの設定
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Width = sizeInBytes;
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	// 実際にリソース（バッファ）を作成
	Microsoft::WRL::ComPtr<ID3D12Resource> resource = nullptr;
	HRESULT hr = device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_COMMON, // 初期状態（読み取り用）
		nullptr,
		IID_PPV_ARGS(&resource)
	);
	if (FAILED(hr)) {
		throw std::runtime_error("CreateBufferResource failed! VRAM might be full or arguments invalid.");
	}

	return resource; // 作ったバッファを返す
}

// DescriptorHeapの作成関数
Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(
	Microsoft::WRL::ComPtr<ID3D12Device> device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible) {
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.Type = heapType;
	desc.NumDescriptors = numDescriptors;
	desc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> heap = nullptr;
	HRESULT hr = device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap));
	assert(SUCCEEDED(hr));
	return heap;
}

// Textureデータを読む
DirectX::ScratchImage LoadTexture(const std::string &filePath) {
    // ファイルパス確認用のログ
    OutputDebugStringA(("LoadTexture: " + filePath + "\n").c_str());

    DirectX::ScratchImage image{};

    // ファイルが存在しない場合の安全なフォールバック
    if (!std::filesystem::exists(filePath)) {
        OutputDebugStringA(("[WARNING] LoadTexture: File not found: " + filePath + ", fallback to 1x1 white texture.\n").c_str());
        HRESULT hrFallback = image.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, 1, 1);
        if (SUCCEEDED(hrFallback)) {
            uint8_t* pixels = image.GetPixels();
            if (pixels) {
                pixels[0] = 255;
                pixels[1] = 255;
                pixels[2] = 255;
                pixels[3] = 255;
            }
        }
        return image;
    }

    std::wstring filePathW = ConvertString(filePath);
    HRESULT hr = E_FAIL;

    // 備考1：DDSファイルに対応する
    if (filePathW.ends_with(L".dds")) {
        // .ddsで終わっていたらDDSとして読み込む。sRGB情報が含まれているのでフラグはNONE
        hr = DirectX::LoadFromDDSFile(filePathW.c_str(), DirectX::DDS_FLAGS_NONE, nullptr, image);
    } else {
        // それ以外は従来通りWIC（PNGやJPGなど）として読み込む
        hr = DirectX::LoadFromWICFile(filePathW.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
    }

    if (FAILED(hr)) {
        OutputDebugStringA(("[WARNING] LoadTexture failed to load file: " + filePath + ", fallback to 1x1 white texture.\n").c_str());
        HRESULT hrFallback = image.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, 1, 1);
        if (SUCCEEDED(hrFallback)) {
            uint8_t* pixels = image.GetPixels();
            if (pixels) {
                pixels[0] = 255;
                pixels[1] = 255;
                pixels[2] = 255;
                pixels[3] = 255;
            }
        }
        return image;
    }

    // 備考2：圧縮フォーマットか判定してミップマップ生成を避ける
    DirectX::ScratchImage mipImages{};
    if (DirectX::IsCompressed(image.GetMetadata().format)) {
        // 圧縮フォーマットならそのまま使う（DirectXTexが直接のミップマップ生成に非対応なため）
        mipImages = std::move(image);
    } else {
        // 非圧縮ならミップマップを作成する
        hr = DirectX::GenerateMipMaps(
            image.GetImages(), image.GetImageCount(), image.GetMetadata(),
            DirectX::TEX_FILTER_SRGB, 4, mipImages); // 第5引数の 0(MAX) や 4 など任意に変更可能
        if (FAILED(hr)) {
            return image;
        }
    }

    return mipImages;
}

// DirectX12のTextureResourceを作る
Microsoft::WRL::ComPtr<ID3D12Resource> CreateTextureResource(Microsoft::WRL::ComPtr<ID3D12Device> device, const DirectX::TexMetadata &metadata) {
	// metadataをもとにResourceの設定
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = UINT(metadata.width);// 横幅
	resourceDesc.Height = UINT(metadata.height);// 鬮倥＆
	resourceDesc.MipLevels = UINT(metadata.mipLevels);// mipmapの数
	resourceDesc.DepthOrArraySize = UINT(metadata.arraySize);// 奥行き or 配列Textureの配列数
	resourceDesc.Format = metadata.format;// TextureのFormat
	resourceDesc.SampleDesc.Count = 1;// サンプリングカウント
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION(metadata.dimension);// Textureの次元（2次元）

	// 利用するHeapの設定
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;// 細かい設定を行う

	// Resourceの生成
	Microsoft::WRL::ComPtr<ID3D12Resource> resource = nullptr;
	HRESULT hr = device->CreateCommittedResource(
		&heapProperties,// Heapの設定
		D3D12_HEAP_FLAG_NONE,// Heapの特殊な設定。今回はなし
		&resourceDesc,// Resourceの設定
		D3D12_RESOURCE_STATE_COPY_DEST,// 初回のResourceState
		nullptr,//Clear最適値。今回は使わない
		IID_PPV_ARGS(&resource)
	);
	if (FAILED(hr)) {
		throw std::runtime_error("CreateTextureResource failed to create committed resource.");
	}
	return resource;
}

// 戻り値を破棄してはならないのでこれを付ける
[[nodiscard]]
// TextureResourceにデータを転送する
Microsoft::WRL::ComPtr<ID3D12Resource> UploadTextureData(Microsoft::WRL::ComPtr<ID3D12Resource> texture, const DirectX::ScratchImage &mipImages, Microsoft::WRL::ComPtr<ID3D12Device> device, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList) {
	std::vector<D3D12_SUBRESOURCE_DATA>subresources;
	// 読み込んだデータからDirectX12用のSubresourceの配列を作成
	DirectX::PrepareUpload(device.Get(), mipImages.GetImages(), mipImages.GetImageCount(), mipImages.GetMetadata(), subresources);
	// IntermediateResourceに必要なサイズを計算する
	uint64_t intermediateSize = GetRequiredIntermediateSize(texture.Get(), 0, UINT(subresources.size()));
	// 計算したサイズでIntermediateResourceを作る
	Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource = CreateBufferResource(device, intermediateSize);
	// データ転送をコマンドに積む
	UpdateSubresources(commandList.Get(), texture.Get(), intermediateResource.Get(), 0, 0, UINT(subresources.size()), subresources.data());
	// Textureへの転送後に利用できるよう、D3D12_RESOURCE_STATE_COPY_DESTからD3D12_RESOURCE_STATE_GENERIC_READへResourceStateを変更する
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = texture.Get();
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
	commandList->ResourceBarrier(1, &barrier);
	return intermediateResource;
}

Microsoft::WRL::ComPtr<ID3D12Resource> CreateDepthStencilTextureResource(Microsoft::WRL::ComPtr<ID3D12Device> device, int32_t width, int32_t height) {
	// 生成するResourceの設定
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = width;
	resourceDesc.Height = height;
	resourceDesc.MipLevels = 1;
	resourceDesc.DepthOrArraySize = 1;// 螂･陦後″
	resourceDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;// 2次元
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	// 利用するHeapの設定
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

	// 深度値のクリア設定
	D3D12_CLEAR_VALUE depthClearValue{};
	depthClearValue.DepthStencil.Depth = 1.0f;
	depthClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;// フォーマットをResourceと合わせる

	// Resourceの設定
	Microsoft::WRL::ComPtr<ID3D12Resource> resource = nullptr;
	HRESULT hr = device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&depthClearValue,
		IID_PPV_ARGS(&resource)
	);

	assert(SUCCEEDED(hr));
	return resource;
}

// DescriptorHandleを取得する(CPU)
D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap, uint32_t descriptorSize, uint32_t index) {
	D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
	handleCPU.ptr += (descriptorSize * index);
	return handleCPU;
}

// DescriptorHandleを取得する(GPU)
D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap, uint32_t descriptorSize, uint32_t index) {
	D3D12_GPU_DESCRIPTOR_HANDLE handleGPU = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
	handleGPU.ptr += (descriptorSize * index);
	return handleGPU;
}



void CreateSphereMesh(std::vector<VertexData> &vertices, std::vector<uint32_t> &indices, float radius, int latDiv, int lonDiv) {
	// 緯度の分割数: 上から下へ何段に分割するか
	// 経度の分割数: 横に何分割するか（赤道の輪切りみたいなイメージ）

	// 頂点の生成（緯度方向にループ）
	for(int lat = 0; lat <= latDiv; ++lat) {
		float theta = lat * float(M_PI) / float(latDiv); // 緯度の角度（0 ~ π）
		float sinTheta = sinf(theta);
		float cosTheta = cosf(theta);

		// 経度方向にループ
		for(int lon = 0; lon <= lonDiv; ++lon) {
			float phi = lon * 2.0f * float(M_PI) / float(lonDiv); // 経度の角度（0 ~ 2π）
			float sinPhi = sinf(phi);
			float cosPhi = cosf(phi);

			// 球面のx, y, z座標を求める
			float x = cosPhi * sinTheta;
			float y = cosTheta;
			float z = sinPhi * sinTheta;

			// 頂点データを作成
			VertexData v{};
			v.position = { radius * x, radius * y, radius * z, 1.0f }; // 球の表面上の点
			v.normal = {v.position.x / radius, v.position.y / radius, v.position.z / radius};
            v.texcoord = { (float)lon / lonDiv, (float)lat / latDiv };
            v.color = {1.0f, 1.0f, 1.0f, 1.0f};
            vertices.push_back(v); // 鬆らせ繝ｪ繧ｹ繝医↓霑ｽ蜉
		}
	}
	// 三角形インデックスの生成（頂点をつなぐ）
	for(int lat = 0; lat < latDiv; ++lat) {
		for(int lon = 0; lon < lonDiv; ++lon) {
			// 現在の行・列から頂点の番号を計算
			int first = lat * (lonDiv + 1) + lon;
			int second = first + lonDiv + 1;

			// 二つの三角形を使って四角形を埋める
			indices.push_back(first);         // 左下
			indices.push_back(first + 1);     // 右下
			indices.push_back(second);        // 左上

			indices.push_back(second);        // 左上
			indices.push_back(first + 1);     // 右下
			indices.push_back(second + 1);    // 右上
		}
	}
}

Node ReadNode(aiNode *node) {
    Node result;

    aiVector3D scale, translate;
    aiQuaternion rotate;
    node->mTransformation.Decompose(scale, rotate, translate);
    result.transform.scale = { scale.x, scale.y, scale.z };
    result.transform.rotate = { rotate.x, rotate.y, rotate.z, rotate.w };
    result.transform.translate = { translate.x, translate.y, translate.z };
    result.localMatrix = TransformFunctions::MakeAffineMatrix(result.transform.scale, result.transform.rotate, result.transform.translate);

    result.name = node->mName.C_Str();
    result.children.resize(node->mNumChildren);

    for (uint32_t childIndex = 0; childIndex < node->mNumChildren; ++childIndex) {
        result.children[childIndex] = ReadNode(node->mChildren[childIndex]);
    }

    return result;
}

ModelData LoadModelFile(const std::string &directoryPath, const std::string &filename) {
    ModelData modelData;
    Assimp::Importer importer;
    std::string filePath = directoryPath + "/" + filename;

    // 1. ファイルの読み込み
    const aiScene *scene = nullptr;
    try {
        scene = importer.ReadFile(filePath.c_str(),
                                  aiProcess_Triangulate |
                                  aiProcess_FlipUVs |
                                  aiProcess_ConvertToLeftHanded |
                                  aiProcess_GenSmoothNormals |
                                  aiProcess_GlobalScale);
    } catch (...) {
        scene = nullptr;
    }

    // メッシュがないファイル（アニメーション専用ファイルや無効なファイル）の場合は空のModelDataを安全に返却
    if (!scene || !scene->HasMeshes() || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !scene->mRootNode) {
        return modelData;
    }

    // 2. メッシュの解析（備考に基づき、全メッシュをループ）
    for (uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
        aiMesh *mesh = scene->mMeshes[meshIndex];
        if (!mesh) continue;

        // MultiMesh/MultiMaterial対応のため、頂点の開始位置を記録しておく
        uint32_t vertexOffset = static_cast<uint32_t>(modelData.vertices.size());

        bool hasNormals = mesh->HasNormals();
        bool hasTexCoords = mesh->HasTextureCoords(0);

        // 頂点データの解析
        for (uint32_t vIndex = 0; vIndex < mesh->mNumVertices; ++vIndex) {
            aiVector3D &position = mesh->mVertices[vIndex];
            aiVector3D normal = hasNormals ? mesh->mNormals[vIndex] : aiVector3D(0.0f, 1.0f, 0.0f);
            aiVector3D texcoord = hasTexCoords ? mesh->mTextureCoords[0][vIndex] : aiVector3D(0.0f, 0.0f, 0.0f);

            VertexData vertex;
            vertex.position = {position.x, position.y, position.z, 1.0f};
            vertex.normal = {normal.x, normal.y, normal.z};
            vertex.texcoord = {texcoord.x, texcoord.y};
            vertex.color = {1.0f, 1.0f, 1.0f, 1.0f};
            modelData.vertices.push_back(vertex);
        }

        // Bone解析
        for (uint32_t boneIndex = 0; boneIndex < mesh->mNumBones; ++boneIndex) {
            aiBone* bone = mesh->mBones[boneIndex];
            if (!bone) continue;
            std::string jointName = bone->mName.C_Str();
            JointWeightData& weightData = modelData.skinClusterData[jointName];

            aiMatrix4x4 bindPoseMatrixAssimp = bone->mOffsetMatrix.Inverse(); // BindPoseMatrixに戻す
            aiVector3D scale, translate;
            aiQuaternion rotate;
            bindPoseMatrixAssimp.Decompose(scale, rotate, translate); // 成分を抽出
            Matrix4x4 bindPoseMatrix = TransformFunctions::MakeAffineMatrix(
                { scale.x, scale.y, scale.z },
                { rotate.x, rotate.y, rotate.z, rotate.w },
                { translate.x, translate.y, translate.z }
            );
            // InverseBindPoseMatrixにする
            weightData.inverseBindPoseMatrix = TransformFunctions::Inverse(bindPoseMatrix);

            for (uint32_t weightIndex = 0; weightIndex < bone->mNumWeights; ++weightIndex) {
                weightData.vertexWeights.push_back({
                    bone->mWeights[weightIndex].mWeight,
                    bone->mWeights[weightIndex].mVertexId + vertexOffset // vertexOffsetを加算する
                });
            }
        }

        // インデックス(Face)の解析
        for (uint32_t faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex) {
            aiFace &face = mesh->mFaces[faceIndex];
            if (face.mNumIndices == 3) {
                for (uint32_t element = 0; element < face.mNumIndices; ++element) {
                    uint32_t vertexIndex = face.mIndices[element];
                    modelData.indices.push_back(vertexIndex + vertexOffset);
                }
            } else if (face.mNumIndices > 3) {
                for (uint32_t element = 1; element + 1 < face.mNumIndices; ++element) {
                    modelData.indices.push_back(face.mIndices[0] + vertexOffset);
                    modelData.indices.push_back(face.mIndices[element] + vertexOffset);
                    modelData.indices.push_back(face.mIndices[element + 1] + vertexOffset);
                }
            }
        }
    }

    // 3. マテリアルの解析（Diffuse / BaseColor テクスチャを取得）
    for (uint32_t materialIndex = 0; materialIndex < scene->mNumMaterials; ++materialIndex) {
        aiMaterial *material = scene->mMaterials[materialIndex];
        if (!material) continue;
        aiString textureFilePath;
        if (material->GetTextureCount(aiTextureType_DIFFUSE) != 0) {
            material->GetTexture(aiTextureType_DIFFUSE, 0, &textureFilePath);
            modelData.material.textureFilePath = directoryPath + "/" + textureFilePath.C_Str();
        } else if (material->GetTextureCount(aiTextureType_BASE_COLOR) != 0) {
            material->GetTexture(aiTextureType_BASE_COLOR, 0, &textureFilePath);
            modelData.material.textureFilePath = directoryPath + "/" + textureFilePath.C_Str();
        }
    }

    if (scene->mRootNode) {
        modelData.rootNode = ReadNode(scene->mRootNode);
    }

    return modelData;
}

MaterialData LoadMaterialTemplateFile(const std::string &directoryPath, const std::string &filename) {
	MaterialData materialData;// 讒狗ｯ峨☆繧貴aterialData
	std::string line;//　ファイルから読んだ1行目を格納する
	std::ifstream file(directoryPath + "/" + filename);// ファイルを開く
	assert(file.is_open());// 開けなかったら止める

	while(std::getline(file, line)) {
		std::string identifier;
		std::istringstream s(line);
		s >> identifier;

		// identifierに応じた処理
		if(identifier == "map_Kd") {
			std::string textureFilename;
			s >> textureFilename;
			// 連結してファイルパスにする
			materialData.textureFilePath = directoryPath + "/" + textureFilename;

			OutputDebugStringA(("Material Texture Path: " + materialData.textureFilePath + "\n").c_str());
		}
	}
	return materialData;
}

SoundData SoundLoadWave(const char *filename) {
	//HRESULT result;

	/*繝輔ぃ繧､繝ｫ繧ｪ繝ｼ繝励Φ
	*********************************************************/

	// ファイル入力ストリームのインスタンス
	std::ifstream file;
	// .wav繝輔ぃ繧､繝ｫ繧偵ヰ繧､繝翫Μ繝｢繝ｼ繝峨〒髢九￥
	file.open(filename, std::ios_base::binary);
	// ファイルオープン失敗を検出する
	assert(file.is_open());

	/*.wavデータ読み込み
	*********************************************************/

	// RIFFヘッダーの読み込み
	RiffHeader riff;
	file.read((char *)&riff, sizeof(riff));
	OutputDebugStringA(std::format("Read RIFF ID: {}\n", std::string(riff.chunk.id, 4)).c_str());
	// タイプがRIFFかチェック
	if(strncmp(riff.chunk.id, "RIFF", 4) != 0) {
		assert(0);
	}
	// タイプがWAVEかチェック
	if(strncmp(riff.type, "WAVE", 4) != 0) {
		assert(0);
	}

	// Formatチャンク読み込み
	FormatChunk format = {};
	// fmtチャンクを探すループ
	while(true) {
		// 繝√Ε繝ｳ繧ｯ繝倥ャ繝繝ｼ繧定ｪｭ繧
		file.read((char *)&format.chunk, sizeof(ChunkHeader));

		// チャンクIDが"fmt " ならbreak
		if(strncmp(format.chunk.id, "fmt ", 4) == 0) {
			break;
		}

		// それ以外ならスキップ
		file.seekg(format.chunk.size, std::ios_base::cur);
	}
	// チャンク本体の読み込み
	assert(format.chunk.size <= sizeof(format.fmt));
	file.read((char *)&format.fmt, format.chunk.size);
	// Dataチャンクの読み込み
	ChunkHeader data;
	file.read((char *)&data, sizeof(data));
	// JUNKチャンクを検出した場合
	if(strncmp(data.id, "JUNK", 4) == 0) {
		// 読み取り位置をJUNKチャンクの終わりまで進める
		file.seekg(data.size, std::ios_base::cur);
		// 再読み込み
		file.read((char *)&data, sizeof(data));
	}

	if(strncmp(data.id, "data", 4) != 0) {
		assert(0);
	}

	// Dataチャンクのデータ部分読み込み
    auto pBuffer = std::make_unique<char[]>(data.size);
    file.read(pBuffer.get(), data.size);

	// waveファイルを閉じる
	file.close();

	/*.読み込んだ音声データをreturn
	*********************************************************/

	// returnするための音声データ
	SoundData soundData = {};

	soundData.wfex = format.fmt;
    soundData.pBuffer.reset(reinterpret_cast<BYTE *>(pBuffer.release()));
	soundData.bufferSize = data.size;

	return soundData;

}

void SoundUnload(SoundData *soundData) {
	// バッファのメモリを解放
    soundData->pBuffer.reset();

	soundData->bufferSize = 0;
	soundData->wfex = {};
}

bool IsKeyHeld(BYTE keys) {
	if(keys) {
		return true;
	}
	return false;
}

bool IsKeyReleased(BYTE keys, BYTE preKeys) {
	if(!keys && preKeys) {
		return true;
	}
	return false;
}

bool IsKeyPressed(BYTE keys, BYTE preKeys) {
	if(keys && !preKeys) {
		return true;
	}
	return false;
}

bool IsKeyUp(BYTE keys) {
	if(!keys) {
		return true;
	}
	return false;
}

SoundData SoundLoadMediaFoundation(const char *filename) {
    HRESULT hr;
    Microsoft::WRL::ComPtr<IMFSourceReader> pSourceReader;

    // 1. SourceReaderの作成
    std::wstring wFilename = ConvertString(filename);
    hr = MFCreateSourceReaderFromURL(wFilename.c_str(), nullptr, &pSourceReader);
    assert(SUCCEEDED(hr));

    // 2. 出力形式をPCM（解凍後の生データ）に設定
    Microsoft::WRL::ComPtr<IMFMediaType> pTargetMediaType;
    MFCreateMediaType(&pTargetMediaType);
    pTargetMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    pTargetMediaType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
    pSourceReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, pTargetMediaType.Get());

    // 3. 最終的な波形フォーマットを取得
    Microsoft::WRL::ComPtr<IMFMediaType> pActualMediaType;
    pSourceReader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, &pActualMediaType);

    WAVEFORMATEX *pWfex;
    UINT32 wfexSize;
    MFCreateWaveFormatExFromMFMediaType(pActualMediaType.Get(), &pWfex, &wfexSize);

    // 4. 全てのサンプルを読み込んでバッファに格納
    std::vector<BYTE> audioData;
    while (true) {
        DWORD dwFlags = 0;
        Microsoft::WRL::ComPtr<IMFSample> pSample;
        hr = pSourceReader->ReadSample(MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, nullptr, &dwFlags, nullptr, &pSample);
        if (FAILED(hr) || (dwFlags & MF_SOURCE_READERF_ENDOFSTREAM))
            break;

        Microsoft::WRL::ComPtr<IMFMediaBuffer> pBuffer;
        pSample->ConvertToContiguousBuffer(&pBuffer);

        BYTE *pData = nullptr;
        DWORD cbCurrentLength = 0;
        pBuffer->Lock(&pData, nullptr, &cbCurrentLength);
        audioData.insert(audioData.end(), pData, pData + cbCurrentLength);
        pBuffer->Unlock();
    }

    // SoundData讒矩菴薙↓隧ｰ繧√※霑斐☆
    SoundData soundData = {};
    soundData.wfex = *pWfex;
    soundData.pBuffer = std::make_unique<BYTE[]>(audioData.size());
    soundData.bufferSize = (unsigned int)audioData.size();
    std::memcpy(soundData.pBuffer.get(), audioData.data(), audioData.size());

    CoTaskMemFree(pWfex); // MFが生成したフォーマット構造体を解放
    return soundData;
}

Microsoft::WRL::ComPtr<ID3D12Resource> CreateRenderTextureResource(
    Microsoft::WRL::ComPtr<ID3D12Device> device,
    uint32_t width,
    uint32_t height,
    DXGI_FORMAT format,
    const Vector4 &clearColor,
    D3D12_RESOURCE_STATES initialState) {

    assert(device != nullptr);

    // 生成するResourceの設定
    D3D12_RESOURCE_DESC resourceDesc{};
    resourceDesc.Width = width;
    resourceDesc.Height = height;
    resourceDesc.MipLevels = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.Format = format;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

    // 備考1: RenderTargetとして利用可能にする特殊なフラグ
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    // 利用するHeapの設定
    D3D12_HEAP_PROPERTIES heapProperties{};
    // 備考2: 当然VRAM上に作る (DEFAULT)
    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

    // クリア時の色を設定（レンダーターゲット生成時にはこれが必要です）
    D3D12_CLEAR_VALUE clearValue{};
    clearValue.Format = format;
    clearValue.Color[0] = clearColor.x; // R
    clearValue.Color[1] = clearColor.y; // G
    clearValue.Color[2] = clearColor.z; // B
    clearValue.Color[3] = clearColor.w; // A

    Microsoft::WRL::ComPtr<ID3D12Resource> resource = nullptr;
    HRESULT hr = device->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        initialState,
        &clearValue,
        IID_PPV_ARGS(&resource));
    assert(SUCCEEDED(hr));

    return resource;
}

void CreateBoxMesh(std::vector<SkyboxVertexData> &vertices, std::vector<uint32_t> &indices) {
    vertices.resize(24);

    // --- 頂点座標の定義 ---
    // 蜿ｳ髱｢ (+X)
    vertices[0].position = {1.0f, 1.0f, 1.0f, 1.0f};
    vertices[1].position = {1.0f, 1.0f, -1.0f, 1.0f};
    vertices[2].position = {1.0f, -1.0f, 1.0f, 1.0f};
    vertices[3].position = {1.0f, -1.0f, -1.0f, 1.0f};
    // 蟾ｦ髱｢ (-X)
    vertices[4].position = {-1.0f, 1.0f, -1.0f, 1.0f};
    vertices[5].position = {-1.0f, 1.0f, 1.0f, 1.0f};
    vertices[6].position = {-1.0f, -1.0f, -1.0f, 1.0f};
    vertices[7].position = {-1.0f, -1.0f, 1.0f, 1.0f};
    // 蜑埼擇 (+Z)
    vertices[8].position = {-1.0f, 1.0f, 1.0f, 1.0f};
    vertices[9].position = {1.0f, 1.0f, 1.0f, 1.0f};
    vertices[10].position = {-1.0f, -1.0f, 1.0f, 1.0f};
    vertices[11].position = {1.0f, -1.0f, 1.0f, 1.0f};
    // 蠕碁擇 (-Z)
    vertices[12].position = {1.0f, 1.0f, -1.0f, 1.0f};
    vertices[13].position = {-1.0f, 1.0f, -1.0f, 1.0f};
    vertices[14].position = {1.0f, -1.0f, -1.0f, 1.0f};
    vertices[15].position = {-1.0f, -1.0f, -1.0f, 1.0f};
    // 荳企擇 (+Y)
    vertices[16].position = {-1.0f, 1.0f, -1.0f, 1.0f};
    vertices[17].position = {1.0f, 1.0f, -1.0f, 1.0f};
    vertices[18].position = {-1.0f, 1.0f, 1.0f, 1.0f};
    vertices[19].position = {1.0f, 1.0f, 1.0f, 1.0f};
    // 荳矩擇 (-Y)
    vertices[20].position = {-1.0f, -1.0f, 1.0f, 1.0f};
    vertices[21].position = {1.0f, -1.0f, 1.0f, 1.0f};
    vertices[22].position = {-1.0f, -1.0f, -1.0f, 1.0f};
    vertices[23].position = {1.0f, -1.0f, -1.0f, 1.0f};

    // --- インデックスの定義（外側を向く順！）---
    // 例 [0,1,2][2,1,3] のパターンで計6個
    for (uint32_t i = 0; i < 6; ++i) {
        uint32_t offset = i * 4;
        indices.push_back(offset + 0);
        indices.push_back(offset + 1);
        indices.push_back(offset + 2);
        indices.push_back(offset + 2);
        indices.push_back(offset + 1);
        indices.push_back(offset + 3);
    }
}


#include "LogManager.h"
#include <set>

Animation LoadAnimationFile(const std::string& directoryPath, const std::string& filename) {
    Animation animation; // create animation
    Assimp::Importer importer;
    std::string filePath = directoryPath + "/" + filename;
    const aiScene* scene = importer.ReadFile(filePath.c_str(), aiProcess_ConvertToLeftHanded | aiProcess_GlobalScale);
    assert(scene->mNumAnimations != 0); // assert animation exists
    aiAnimation* animationAssimp = scene->mAnimations[0]; // use first animation
    animation.duration = float(animationAssimp->mDuration / animationAssimp->mTicksPerSecond); // convert to seconds

    // loop channels for node animations
    for (uint32_t channelIndex = 0; channelIndex < animationAssimp->mNumChannels; ++channelIndex) {
        aiNodeAnim* nodeAnimationAssimp = animationAssimp->mChannels[channelIndex];
        NodeAnimation& nodeAnimation = animation.nodeAnimations[nodeAnimationAssimp->mNodeName.C_Str()];
        
        // Translate
        for (uint32_t keyIndex = 0; keyIndex < nodeAnimationAssimp->mNumPositionKeys; ++keyIndex) {
            aiVectorKey& keyAssimp = nodeAnimationAssimp->mPositionKeys[keyIndex];
            KeyframeVector3 keyframe;
            keyframe.time = float(keyAssimp.mTime / animationAssimp->mTicksPerSecond); // convert to seconds
            keyframe.value = {keyAssimp.mValue.x, keyAssimp.mValue.y, keyAssimp.mValue.z}; // Converter already handled by Assimp
            nodeAnimation.translate.push_back(keyframe);
        }

        // Rotate
        for (uint32_t keyIndex = 0; keyIndex < nodeAnimationAssimp->mNumRotationKeys; ++keyIndex) {
            aiQuatKey& keyAssimp = nodeAnimationAssimp->mRotationKeys[keyIndex];
            KeyframeQuaternion keyframe;
            keyframe.time = float(keyAssimp.mTime / animationAssimp->mTicksPerSecond); // convert to seconds
            keyframe.value = {keyAssimp.mValue.x, keyAssimp.mValue.y, keyAssimp.mValue.z, keyAssimp.mValue.w}; // Converter already handled by Assimp
            nodeAnimation.rotate.push_back(keyframe);
        }

        // Scale
        for (uint32_t keyIndex = 0; keyIndex < nodeAnimationAssimp->mNumScalingKeys; ++keyIndex) {
            aiVectorKey& keyAssimp = nodeAnimationAssimp->mScalingKeys[keyIndex];
            KeyframeVector3 keyframe;
            keyframe.time = float(keyAssimp.mTime / animationAssimp->mTicksPerSecond); // convert to seconds
            keyframe.value = {keyAssimp.mValue.x, keyAssimp.mValue.y, keyAssimp.mValue.z};
            nodeAnimation.scale.push_back(keyframe);
        }
    }
    return animation;
}

Animation LoadAnimationFile(const std::string& directoryPath, const std::string& filename, const std::string& animationName) {
    Animation animation;
    Assimp::Importer importer;
    std::string filePath = directoryPath + "/" + filename;
    const aiScene* scene = importer.ReadFile(filePath.c_str(), aiProcess_ConvertToLeftHanded | aiProcess_GlobalScale);
    assert(scene && scene->mNumAnimations != 0);

    aiAnimation* animationAssimp = nullptr;
    for (uint32_t i = 0; i < scene->mNumAnimations; ++i) {
        if (std::string(scene->mAnimations[i]->mName.C_Str()) == animationName) {
            animationAssimp = scene->mAnimations[i];
            break;
        }
    }

    if (!animationAssimp) {
        animationAssimp = scene->mAnimations[0]; // Fallback to first animation if name not found
    }

    animation.duration = float(animationAssimp->mDuration / animationAssimp->mTicksPerSecond);

    for (uint32_t channelIndex = 0; channelIndex < animationAssimp->mNumChannels; ++channelIndex) {
        aiNodeAnim* nodeAnimationAssimp = animationAssimp->mChannels[channelIndex];
        NodeAnimation& nodeAnimation = animation.nodeAnimations[nodeAnimationAssimp->mNodeName.C_Str()];
        
        for (uint32_t keyIndex = 0; keyIndex < nodeAnimationAssimp->mNumPositionKeys; ++keyIndex) {
            aiVectorKey& keyAssimp = nodeAnimationAssimp->mPositionKeys[keyIndex];
            KeyframeVector3 keyframe;
            keyframe.time = float(keyAssimp.mTime / animationAssimp->mTicksPerSecond);
            keyframe.value = {keyAssimp.mValue.x, keyAssimp.mValue.y, keyAssimp.mValue.z};
            nodeAnimation.translate.push_back(keyframe);
        }

        for (uint32_t keyIndex = 0; keyIndex < nodeAnimationAssimp->mNumRotationKeys; ++keyIndex) {
            aiQuatKey& keyAssimp = nodeAnimationAssimp->mRotationKeys[keyIndex];
            KeyframeQuaternion keyframe;
            keyframe.time = float(keyAssimp.mTime / animationAssimp->mTicksPerSecond);
            keyframe.value = {keyAssimp.mValue.x, keyAssimp.mValue.y, keyAssimp.mValue.z, keyAssimp.mValue.w};
            nodeAnimation.rotate.push_back(keyframe);
        }

        for (uint32_t keyIndex = 0; keyIndex < nodeAnimationAssimp->mNumScalingKeys; ++keyIndex) {
            aiVectorKey& keyAssimp = nodeAnimationAssimp->mScalingKeys[keyIndex];
            KeyframeVector3 keyframe;
            keyframe.time = float(keyAssimp.mTime / animationAssimp->mTicksPerSecond);
            keyframe.value = {keyAssimp.mValue.x, keyAssimp.mValue.y, keyAssimp.mValue.z};
            nodeAnimation.scale.push_back(keyframe);
        }
    }

    // デバッグログ: ロードされたチャンネル名を出力
    std::string keysLog = "Loaded animation [" + animationName + "] channels: ";
    for (const auto& [key, val] : animation.nodeAnimations) {
        keysLog += key + ", ";
    }
    LogManager::GetInstance()->AddLog(LogLevel::Info, keysLog);

    return animation;
}


int32_t CreateJoint(const Node& node, const std::optional<int32_t>& parent, std::vector<Joint>& joints) {
    Joint joint;
    joint.name = node.name;
    joint.localMatrix = node.localMatrix;
    joint.skeletonSpaceMatrix = TransformFunctions::MakeIdentity4x4();
    joint.transform = node.transform;
    joint.defaultTransform = node.transform;
    joint.index = int32_t(joints.size());
    joint.parent = parent;
    joints.push_back(joint);
    for (const Node& child : node.children) {
        int32_t childIndex = CreateJoint(child, joint.index, joints);
        joints[joint.index].children.push_back(childIndex);
    }
    return joint.index;
}

Skeleton CreateSkeleton(const Node& rootNode) {
    Skeleton skeleton;
    skeleton.root = CreateJoint(rootNode, {}, skeleton.joints);

    for (const Joint& joint : skeleton.joints) {
        skeleton.jointMap.emplace(joint.name, joint.index);
    }

    Update(skeleton);
    
    // Log joint count
    

    return skeleton;
}

void Update(Skeleton& skeleton) {
    for (Joint& joint : skeleton.joints) {
        joint.localMatrix = TransformFunctions::MakeAffineMatrix(joint.transform.scale, joint.transform.rotate, joint.transform.translate);
        if (joint.parent) {
            joint.skeletonSpaceMatrix = joint.localMatrix * skeleton.joints[*joint.parent].skeletonSpaceMatrix;
        } else {
            joint.skeletonSpaceMatrix = joint.localMatrix;
        }
    }
}

void ApplyAnimation(Skeleton& skeleton, const Animation& animation, float animationTime) {
    static std::set<std::string> reportedMissingNodes;
    for (Joint& joint : skeleton.joints) {
        if (auto it = animation.nodeAnimations.find(joint.name); it != animation.nodeAnimations.end()) {
            const NodeAnimation& rootNodeAnimation = (*it).second;
            if (!rootNodeAnimation.translate.empty()) {
                joint.transform.translate = CalculateValue(rootNodeAnimation.translate, animationTime);
            } else {
                joint.transform.translate = joint.defaultTransform.translate;
            }

            if (!rootNodeAnimation.rotate.empty()) {
                joint.transform.rotate = CalculateValue(rootNodeAnimation.rotate, animationTime);
            } else {
                joint.transform.rotate = joint.defaultTransform.rotate;
            }

            if (!rootNodeAnimation.scale.empty()) {
                joint.transform.scale = CalculateValue(rootNodeAnimation.scale, animationTime);
            } else {
                joint.transform.scale = joint.defaultTransform.scale;
            }
        } else {
            joint.transform = joint.defaultTransform;
            if (reportedMissingNodes.find(joint.name) == reportedMissingNodes.end()) {
                reportedMissingNodes.insert(joint.name);
                LogManager::GetInstance()->AddLog(LogLevel::Info, "Animation channel not found for joint: " + joint.name);
            }
        }
    }
}


SkinCluster CreateSkinCluster(Microsoft::WRL::ComPtr<ID3D12Device> device, const Skeleton& skeleton, const ModelData& modelData) {
    SkinCluster skinCluster;

    // paletteResourceの生成 (関節数分のマトリックス配列)
    skinCluster.paletteResource = CreateBufferResource(device, sizeof(WellForGPU) * skeleton.joints.size());
    WellForGPU* mappedPalette = nullptr;
    skinCluster.paletteResource->Map(0, nullptr, reinterpret_cast<void**>(&mappedPalette));
    skinCluster.mappedPalette = {mappedPalette, skeleton.joints.size()};

    // SRVの生成
    SrvManager::GetInstance()->Allocate(&skinCluster.paletteSrvHandle.first, &skinCluster.paletteSrvHandle.second);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Buffer.FirstElement = 0;
    srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
    srvDesc.Buffer.NumElements = UINT(skeleton.joints.size());
    srvDesc.Buffer.StructureByteStride = sizeof(WellForGPU);

    device->CreateShaderResourceView(skinCluster.paletteResource.Get(), &srvDesc, skinCluster.paletteSrvHandle.first);

    // influenceResourceの生成 (頂点ごとのウェイトデータ)
    skinCluster.influenceResource = CreateBufferResource(device, sizeof(VertexInfluence) * modelData.vertices.size());
    VertexInfluence* mappedInfluence = nullptr;
    skinCluster.influenceResource->Map(0, nullptr, reinterpret_cast<void**>(&mappedInfluence));
    std::memset(mappedInfluence, 0, sizeof(VertexInfluence) * modelData.vertices.size());
    skinCluster.mappedInfluence = {mappedInfluence, modelData.vertices.size()};

    skinCluster.influenceBufferView.BufferLocation = skinCluster.influenceResource->GetGPUVirtualAddress();
    skinCluster.influenceBufferView.SizeInBytes = UINT(sizeof(VertexInfluence) * modelData.vertices.size());
    skinCluster.influenceBufferView.StrideInBytes = sizeof(VertexInfluence);

    // InverseBindPoseMatrix の保存
    skinCluster.inverseBindPoseMatrices.resize(skeleton.joints.size());
    std::generate(skinCluster.inverseBindPoseMatrices.begin(), skinCluster.inverseBindPoseMatrices.end(), TransformFunctions::MakeIdentity4x4);

    // ウェイト情報のパース
    for (const auto& jointWeight : modelData.skinClusterData) {
        auto it = skeleton.jointMap.find(jointWeight.first);
        if (it == skeleton.jointMap.end()) continue; // そのボーンは存在しない

        int32_t jointIndex = it->second;
        skinCluster.inverseBindPoseMatrices[jointIndex] = jointWeight.second.inverseBindPoseMatrix;

        for (const auto& weightInfo : jointWeight.second.vertexWeights) {
            uint32_t vIndex = weightInfo.vertexIndex;
            if (vIndex >= modelData.vertices.size()) continue;

            // 空いているウェイトスロットを探す
            for (uint32_t slot = 0; slot < kNumMaxInfluence; ++slot) {
                if (skinCluster.mappedInfluence[vIndex].weights[slot] == 0.0f) {
                    skinCluster.mappedInfluence[vIndex].weights[slot] = weightInfo.weight;
                    skinCluster.mappedInfluence[vIndex].jointIndices[slot] = jointIndex;
                    break;
                }
            }
        }
    }

    return skinCluster;
}



void Update(SkinCluster& skinCluster, const Skeleton& skeleton) {
    for (size_t jointIndex = 0; jointIndex < skeleton.joints.size(); ++jointIndex) {
        assert(jointIndex < skinCluster.inverseBindPoseMatrices.size());
        
        Matrix4x4 inverseBindPose = skinCluster.inverseBindPoseMatrices[jointIndex];
        Matrix4x4 skeletonSpaceMatrix = skeleton.joints[jointIndex].skeletonSpaceMatrix;

        // パレットに格納する行列 = inverseBindPose * skeletonSpaceMatrix
        Matrix4x4 paletteMatrix = inverseBindPose * skeletonSpaceMatrix;
        skinCluster.mappedPalette[jointIndex].skeletonSpaceMatrix = paletteMatrix;
        
        // 法線用の逆転置行列
        skinCluster.mappedPalette[jointIndex].skeletonSpaceInverseTransposeMatrix = TransformFunctions::Transpose(TransformFunctions::Inverse(paletteMatrix));
    }
}

