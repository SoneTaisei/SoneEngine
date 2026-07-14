#pragma warning(disable: 4828)
#include "UtilityFunctions.h"
#include <map>
#include <fstream>
#include <mutex>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
#ifdef USE_IMGUI


	if(ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
		return true;
	}
#endif // USE_IMGUI

	// メチE��ージに応じてゲーム固有�E処琁E��行う
	switch(msg) {
		// ウィンドウが破壊された
	case WM_DESTROY:
		// OSに応じて、アプリ固有�E終亁E��伝えめE
		PostQuitMessage(0);
		return 0;
	}

	// 標準�EメチE��ージ処琁E��行う
	return DefWindowProc(hwnd, msg, wparam, lparam);
}

void Log(const std::string &message) {
	// チE��チE��出力（従来の動作！E
	OutputDebugStringA(message.c_str());

	// ログファイルを一度だけ作�Eして使ぁE��す（スレチE��セーフ！E
	static std::once_flag s_logInitFlag;
	static std::ofstream s_logStream;
	static std::mutex s_logMutex;

	std::call_once(s_logInitFlag, []() {
		try {
			// logs チE��レクトリを作�E
			std::filesystem::create_directories("logs");

			// 現在の時刻を秒単位に丸める
			auto now = std::chrono::system_clock::now();
			auto nowSeconds = std::chrono::time_point_cast<std::chrono::seconds>(now);

			// ローカルタイムゾーンに変換してフォーマット（�Eコードと同じ書式を使用�E�E
			std::chrono::zoned_time localTime{ std::chrono::current_zone(), nowSeconds };
			std::string dateString = std::format("{:%Y%d_%H%M%S}", localTime);

			// ファイルパスを作�Eして open�E�追記モード！E
			std::string logFilePath = std::string("logs/") + dateString + ".log";
			s_logStream.open(logFilePath, std::ios::app | std::ios::binary);
		} catch(...) {
			// 例外�E無視してチE��チE��出力�Eみ行う�E�ログ失敗してもアプリが止まらなぁE��ぁE��する�E�E
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
	// 時刻を取得して、時刻を名前に入れたファイルを作�E、EumpsチE��レクトリ以下に出劁E
	SYSTEMTIME time;
	GetLocalTime(&time);
	wchar_t filePath[MAX_PATH] = { 0 };

	// チE��レクトリ作�E�E�失敗しても続行！E
	if(!CreateDirectoryW(L"./Dumps", nullptr)) {
		DWORD err = GetLastError();
		if(err != ERROR_ALREADY_EXISTS) {
			Log(std::format("CreateDirectory failed, err:{}\n", err));
		}
	}

	// ファイル名（秒単位！E
	StringCchPrintfW(filePath, MAX_PATH, L"./Dumps/%04d-%02d-%02d_%02d%02d%02d.dmp",
					 time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);

				 // ログにパスを�E劁E
	Log(std::format("ExportDump: target path: {}\n", ConvertString(filePath)));

	// ファイル作�E
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

	// processId(exeID)とクラチE��ュ(例夁Eの発生したthreadIDを取征E
	DWORD processId = GetCurrentProcessId();
	DWORD threadId = GetCurrentThreadId();

	// 設定情報を�E劁E
	MINIDUMP_EXCEPTION_INFORMATION minidumpInformation{};
	minidumpInformation.ThreadId = threadId;
	minidumpInformation.ExceptionPointers = exception;
	minidumpInformation.ClientPointers = TRUE;

	// Dumpを�E力。結果をログに残す
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

	// ほかに関連付けられてぁE��SEH例外ハンドラがあれ�E実行。通常プロセスを終亁E��る、E
	return EXCEPTION_EXECUTE_HANDLER;
}

IDxcBlob *CompileShader(
	// CompilerするShaderファイルへのパス
	const std::wstring &filePath,
	// Compilerに使用するProfile
	const wchar_t *profile,
	// 初期化で生�Eしたも�EめEつ
	IDxcUtils *dxcUtils,
	IDxcCompiler3 *dxcCompiler,
	IDxcIncludeHandler *includeHandler) {

	/*********************************************************
	*1. hlslファイルを読む
	*********************************************************/

	// これからシェーダーをコンパイルする旨をログに出ぁE
	Log(ConvertString(std::format(L"Begin CompileShader, path:{},profile:{}\n", filePath, profile)));
	// hlslファイルを読む
	IDxcBlobEncoding *shaderSource = nullptr;
	HRESULT hr = dxcUtils->LoadFile(filePath.c_str(), nullptr, &shaderSource);
	if (FAILED(hr)) {
		wchar_t buf[MAX_PATH];
		GetCurrentDirectoryW(MAX_PATH, buf);
		Log(ConvertString(std::format(L"Failed to load shader file: {}. hr: {:08X}, CWD: {}\n", filePath, hr, buf)));
	}
	// あきらめなかったら止める
	assert(SUCCEEDED(hr));
	// 読み込んだファイルの冁E��を設定すめE
	DxcBuffer shaderSourceBuffer;
	shaderSourceBuffer.Ptr = shaderSource->GetBufferPointer();
	shaderSourceBuffer.Size = shaderSource->GetBufferSize();
	shaderSourceBuffer.Encoding = DXC_CP_UTF8;// UTF8の斁E��コードであることを通知

	/*********************************************************
	*2.Compileする
	*********************************************************/

	LPCWSTR arguments[] = {
        filePath.c_str(),         // コンパイル対象のhlslファイル吁E
        L"-E", L"main",           // エントリーポイント�E持E��。基本皁E��main以外にはしなぁE
        L"-T", profile,           // ShaderProfileの設宁E
        L"-Zi", L"-Qembed_debug", // チE��チE��用の惁E��を埋め込む
        L"-Od",                   // 最適化を外しておく
        L"-Zpr",                  // メモリレイアウト�E行優允E
        L"-HV", L"2021",          // ☁Eこれを追加�E�EHLSL2021ルールを適用してC++と同じ型名を使えるようにする
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
	// コンパイラエラーではなくdxcが起動できなぁE��どの致命皁E��エラー
	assert(SUCCEEDED(hr));
/*********************************************************
	*3.警告�Eエラーが�EてぁE��ぁE��確誁E
	*********************************************************/

	IDxcBlobUtf8 *shaderError = nullptr;
	shaderResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&shaderError), nullptr);

	// shaderErrorが作られてぁE��、かつ中身の斁E���Eの長さが0ではなぁE��合だけエラーとみなぁE
	if(shaderError != nullptr && shaderError->GetStringLength() != 0) {
		Log(shaderError->GetStringPointer());
		// 警告�Eエラー絶対ダメ
		assert(false);
	}
	/*********************************************************
	*4.Compile結果を受け取って返す
	*********************************************************/

	// コンパイル結果から実行用のバイナリ部刁E��取征E
	IDxcBlob *shaderBlob = nullptr;
	hr = shaderResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
	assert(SUCCEEDED(hr));
	// 成功押したログを�EぁE
	Log(ConvertString(std::format(L"Compile Succeeded, path:{},profile:{}\n", filePath, profile)));
	// もう使わなぁE��ソースを開放
	shaderSource->Release();
	shaderResult->Release();
	// 実行用のバイナリを返却
	return shaderBlob;
}

Microsoft::WRL::ComPtr<ID3D12Resource> CreateBufferResource(Microsoft::WRL::ComPtr<ID3D12Device> device, size_t sizeInBytes) {
	assert(device != nullptr); // 安�EチェチE��

	// アチE�Eロード用のヒ�Eプ�E設定！EPUからGPUにチE�Eタを送る用�E�E
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

	// バッファリソースの設宁E
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Width = sizeInBytes;
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	// 実際にリソース�E�バチE��ァ�E�を作�E
	Microsoft::WRL::ComPtr<ID3D12Resource> resource = nullptr;
	HRESULT hr = device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_COMMON, // 初期状態（読み取り用�E�E
		nullptr,
		IID_PPV_ARGS(&resource)
	);
	if (FAILED(hr)) {
		throw std::runtime_error("CreateBufferResource failed! VRAM might be full or arguments invalid.");
	}

	return resource; // 作ったバチE��ァを返す�E�E
}

// DescriptorHeapの作�E関数
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

// TextureチE�Eタを読む
DirectX::ScratchImage LoadTexture(const std::string &filePath) {
    // ファイルパス確認用のログ
    OutputDebugStringA(("LoadTexture: " + filePath + "\n").c_str());

    DirectX::ScratchImage image{};
    std::wstring filePathW = ConvertString(filePath);
    HRESULT hr;

    // ☁E��E��の持E��1�E�DDSファイルに対応すめE
    if (filePathW.ends_with(L".dds")) {
        // .ddsで終わってぁE��らDDSとして読み込む。sRGB惁E��が含まれてぁE��のでフラグはNONE
        hr = DirectX::LoadFromDDSFile(filePathW.c_str(), DirectX::DDS_FLAGS_NONE, nullptr, image);
    } else {
        // それ以外�E従来通りWIC�E�ENGやJPGなど�E�として読み込む
        hr = DirectX::LoadFromWICFile(filePathW.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
    }
    if (FAILED(hr)) {
        throw std::runtime_error("LoadTexture failed to load file: " + filePath);
    }

    // ☁E��E��の持E��2�E�圧縮フォーマットか判定してミップ�EチE�E生�Eを�Eける
    DirectX::ScratchImage mipImages{};
    if (DirectX::IsCompressed(image.GetMetadata().format)) {
        // 圧縮フォーマットならそのまま使ぁE��EirectXTexが直接のミップ�EチE�E生�Eに非対応なため�E�E
        mipImages = std::move(image);
    } else {
        // 非圧縮ならミチE�Eマップを作�Eする
        hr = DirectX::GenerateMipMaps(
            image.GetImages(), image.GetImageCount(), image.GetMetadata(),
            DirectX::TEX_FILTER_SRGB, 4, mipImages); // 第5引数の 0(MAX) めE4 など任意に変更可能
        assert(SUCCEEDED(hr));
    }

    return mipImages;
}

// DirectX12のTextureResourceを作る
Microsoft::WRL::ComPtr<ID3D12Resource> CreateTextureResource(Microsoft::WRL::ComPtr<ID3D12Device> device, const DirectX::TexMetadata &metadata) {
	// metadataをもとにResourceの設宁E
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = UINT(metadata.width);// 横幁E
	resourceDesc.Height = UINT(metadata.height);// 高さ
	resourceDesc.MipLevels = UINT(metadata.mipLevels);// mipmapの数
	resourceDesc.DepthOrArraySize = UINT(metadata.arraySize);// 奥行き ro 配�ETextureの配�E数
	resourceDesc.Format = metadata.format;// TextureのFormat
	resourceDesc.SampleDesc.Count = 1;// サンプリングカウンチE
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION(metadata.dimension);// Textureの次允E��、E次允E

	// 利用するHeapの設宁E
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;// 細かい設定を行う

	// Resourceの生�E
	Microsoft::WRL::ComPtr<ID3D12Resource> resource = nullptr;
	HRESULT hr = device->CreateCommittedResource(
		&heapProperties,// Heapの設宁E
		D3D12_HEAP_FLAG_NONE,// Heapの特殊な設定。今回はなぁE
		&resourceDesc,// Resourceの設宁E
		D3D12_RESOURCE_STATE_COPY_DEST,// 初回のResourceState、E
		nullptr,//Clear最適値。今回は使わなぁE
		IID_PPV_ARGS(&resource)
	);
	if (FAILED(hr)) {
		throw std::runtime_error("CreateTextureResource failed to create committed resource.");
	}
	return resource;
}

// 戻り値を破損してはならなぁE�Eでこれを付けめE
[[nodiscard]]
// TextureResouorceにチE�Eタを転送すめE
Microsoft::WRL::ComPtr<ID3D12Resource> UploadTextureData(Microsoft::WRL::ComPtr<ID3D12Resource> texture, const DirectX::ScratchImage &mipImages, Microsoft::WRL::ComPtr<ID3D12Device> device, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList) {
	std::vector<D3D12_SUBRESOURCE_DATA>subresources;
	// 読み込んだチE�EタからDirectX12用のSubresourceの配�Eを作�E
	DirectX::PrepareUpload(device.Get(), mipImages.GetImages(), mipImages.GetImageCount(), mipImages.GetMetadata(), subresources);
	// IntermediateResourceに忁E��なサイズを計算すめE
	uint64_t intermediateSize = GetRequiredIntermediateSize(texture.Get(), 0, UINT(subresources.size()));
	// 計算したサイズでIntermediateResourceを作る
	Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource = CreateBufferResource(device, intermediateSize);
	// チE�Eタ転送をコマンドに積�E
	UpdateSubresources(commandList.Get(), texture.Get(), intermediateResource.Get(), 0, 0, UINT(subresources.size()), subresources.data());
	// Tetureへの転送後�E利用できるよう、D3D12_RESOURCE_STATE_COPY_DESTからD3D12_RESOURCE_STATE_GENERIC_READへResourceStateを変更する
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
	// 生�EするResourceの設宁E
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = width;
	resourceDesc.Height = height;
	resourceDesc.MipLevels = 1;
	resourceDesc.DepthOrArraySize = 1;// 奥行き
	resourceDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;// 2次允E
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	// 利用するHeapの設宁E
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

	// 深層値のクリア設宁E
	D3D12_CLEAR_VALUE depthClearValue{};
	depthClearValue.DepthStencil.Depth = 1.0f;
	depthClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;// フォーマットをResourceと合わせる

	// Resourceの設宁E
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

// DescriptorHandleを取得すめECPU)
D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap, uint32_t descriptorSize, uint32_t index) {
	D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
	handleCPU.ptr += (descriptorSize * index);
	return handleCPU;
}

// DescriptorHandleを取得すめEGPU)
D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap, uint32_t descriptorSize, uint32_t index) {
	D3D12_GPU_DESCRIPTOR_HANDLE handleGPU = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
	handleGPU.ptr += (descriptorSize * index);
	return handleGPU;
}



void CreateSphereMesh(std::vector<VertexData> &vertices, std::vector<uint32_t> &indices, float radius, int latDiv, int lonDiv) {
	// 緯度の刁E��数: 上から下へ何段に刁E��るか
	// 経度の刁E��数: 横に何�E割するか（赤道�E輪刁E��みたいなイメージ�E�E

	// 頂点の生�E�E�緯度方向にループ！E
	for(int lat = 0; lat <= latDiv; ++lat) {
		float theta = lat * float(M_PI) / float(latDiv); // 緯度の角度�E�E ~ π�E�E
		float sinTheta = sinf(theta);
		float cosTheta = cosf(theta);

		// 経度方向にルーチE
		for(int lon = 0; lon <= lonDiv; ++lon) {
			float phi = lon * 2.0f * float(M_PI) / float(lonDiv); // 経度の角度�E�E ~ 2π�E�E
			float sinPhi = sinf(phi);
			float cosPhi = cosf(phi);

			// 琁E�Ex, y, z座標を求めめE
			float x = cosPhi * sinTheta;
			float y = cosTheta;
			float z = sinPhi * sinTheta;

			// 頂点チE�Eタを作�E
			VertexData v{};
			v.position = { radius * x, radius * y, radius * z, 1.0f }; // 琁E�E表面上�E点
			v.normal = {v.position.x / radius, v.position.y / radius, v.position.z / radius};
            v.texcoord = { (float)lon / lonDiv, (float)lat / latDiv };
            v.color = {1.0f, 1.0f, 1.0f, 1.0f};
            vertices.push_back(v); // 頂点リストに追加
		}
	}
	// 三角形インチE��クスの生�E�E�頂点をつなぐ！E
	for(int lat = 0; lat < latDiv; ++lat) {
		for(int lon = 0; lon < lonDiv; ++lon) {
			// 現在の行�E列から頂点の番号を計箁E
			int first = lat * (lonDiv + 1) + lon;
			int second = first + lonDiv + 1;

			// 二つの三角形を使って四角形を埋める
			indices.push_back(first);         // 左丁E
			indices.push_back(first + 1);     // 右丁E
			indices.push_back(second);        // 左丁E

			indices.push_back(second);        // 左丁E
			indices.push_back(first + 1);     // 右丁E
			indices.push_back(second + 1);    // 右丁E
		}
	}
}

Node ReadNode(aiNode *node) {
    Node result;

    // 1. 行�Eの取得と転置
    aiMatrix4x4 aiLocalMatrix = node->mTransformation;
    aiLocalMatrix.Transpose(); // 列�Eクトル形式を行�Eクトル形式に転置

    // 2. 行�Eの要素をコピ�E
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            result.localMatrix.m[i][j] = aiLocalMatrix[i][j];
        }
    }

    // 3. 名前と子供�E解极E
    result.name = node->mName.C_Str();          // Node名を格紁E
    result.children.resize(node->mNumChildren); // 子供�E数だけ確俁E

    for (uint32_t childIndex = 0; childIndex < node->mNumChildren; ++childIndex) {
        // 再帰皁E��読んで階層構造を作ってぁE��
        result.children[childIndex] = ReadNode(node->mChildren[childIndex]);
    }

    return result;
}

ModelData LoadModelFile(const std::string &directoryPath, const std::string &filename) {
    ModelData modelData;
    Assimp::Importer importer;
    std::string filePath = directoryPath + "/" + filename;

    // 1. ファイルの読み込み
    // 賁E��にある通り、三角形化、巻き頁E��転、UV反転を指宁E
    const aiScene *scene = importer.ReadFile(filePath.c_str(),
                                             // 1. すべての面を三角形に変換�E�EirectXが理解できる形式にする�E�E
                                             aiProcess_Triangulate |
                                                 // 2. V軸を反転�E�ElTFなどの左下原点を、DirectX標準�E左上原点に合わせる�E�E
                                                 aiProcess_FlipUVs |
                                                 // 3. 右手系から左手系へ変換�E�軸の反転めE��き頁E�E調整をセチE��で行う�E�E
                                                 aiProcess_ConvertToLeftHanded |
                                                 // 4. 法線がなぁE��合に滑らかな法線を生�E�E�ライチE��ング計算に忁E��！E
                                                 aiProcess_GenSmoothNormals |
                                                 // 5. ノ�Eド階層の変形を頂点に焼き付ける！ElTFの回転ズレを直す今回の重要フラグ�E�E
                                                 aiProcess_PreTransformVertices);

    // メチE��ュがなぁE��合�Eエラー
    assert(scene && scene->HasMeshes());

    // 2. メチE��ュの解析（賁E��に基づき、�EメチE��ュをループ！E
    for (uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
        aiMesh *mesh = scene->mMeshes[meshIndex];

        // 法線とTexcoordがなぁE��チE��ュは今回は非対応（賁E��のassert�E�E
        assert(mesh->HasNormals());
        assert(mesh->HasTextureCoords(0));

        // 頂点チE�Eタの解极E
        for (uint32_t vIndex = 0; vIndex < mesh->mNumVertices; ++vIndex) {
            aiVector3D &position = mesh->mVertices[vIndex];
            aiVector3D &normal = mesh->mNormals[vIndex];
            aiVector3D &texcoord = mesh->mTextureCoords[0][vIndex];

            VertexData vertex;
            vertex.position = {position.x, position.y, position.z, 1.0f};
            vertex.normal = {normal.x, normal.y, normal.z};
            vertex.texcoord = {texcoord.x, texcoord.y};

            // 左手系への変換�E�賁E��の通り、Xを反転�E�E
            vertex.position = {position.x, position.y, position.z, 1.0f};
            vertex.normal = {normal.x, normal.y, normal.z};
            vertex.texcoord = {texcoord.x, texcoord.y};
            vertex.color = {1.0f, 1.0f, 1.0f, 1.0f};
            modelData.vertices.push_back(vertex);
        }

        // インチE��クス�E�Eace�E��E解析（賁E���E�Indexed描画に対応させる�E�E
        for (uint32_t faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex) {
            aiFace &face = mesh->mFaces[faceIndex];
            assert(face.mNumIndices == 3); // 三角形のみサポ�EチE

            for (uint32_t element = 0; element < face.mNumIndices; ++element) {
                uint32_t vertexIndex = face.mIndices[element];
                modelData.indices.push_back(vertexIndex);
            }
        }
    }

    // 3. マテリアルの解析（賁E��に基づき、DiffuseチE��スチャを取得！E
    for (uint32_t materialIndex = 0; materialIndex < scene->mNumMaterials; ++materialIndex) {
        aiMaterial *material = scene->mMaterials[materialIndex];
        if (material->GetTextureCount(aiTextureType_DIFFUSE) != 0) {
            aiString textureFilePath;
            material->GetTexture(aiTextureType_DIFFUSE, 0, &textureFilePath);
            modelData.material.textureFilePath = directoryPath + "/" + textureFilePath.C_Str();
        }
    }

	modelData.rootNode = ReadNode(scene->mRootNode);

    return modelData;
}

MaterialData LoadMaterialTemplateFile(const std::string &directoryPath, const std::string &filename) {
	MaterialData materialData;// 構築するMaterialData
	std::string line;//　ファイルから読んだ1行目を格納すめE
	std::ifstream file(directoryPath + "/" + filename);// ファイルを開ぁE
	assert(file.is_open());// 開けなかったら止める

	while(std::getline(file, line)) {
		std::string identifier;
		std::istringstream s(line);
		s >> identifier;

		// identifierに応じた�E琁E
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

	/*ファイルオープン
	*********************************************************/

	// ファイル入力ストリームのインスタンス
	std::ifstream file;
	// .wavファイルをバイナリモードで開く
	file.open(filename, std::ios_base::binary);
	// ファイルオープン失敗を検�Eする
	assert(file.is_open());

	/*.wavチE�Eタ読み込み
	*********************************************************/

	// RIFFヘッダーの読み込み
	RiffHeader riff;
	file.read((char *)&riff, sizeof(riff));
	OutputDebugStringA(std::format("Read RIFF ID: {}\n", std::string(riff.chunk.id, 4)).c_str());
	// タイプがRIFFかチェチE��
	if(strncmp(riff.chunk.id, "RIFF", 4) != 0) {
		assert(0);
	}
	// タイプがWAVEかチェチE��
	if(strncmp(riff.type, "WAVE", 4) != 0) {
		assert(0);
	}

	// Formatチャンク読み込み
	FormatChunk format = {};
	// fmtチャンクを探すルーチE
	while(true) {
		// チャンクヘッダーを読む
		file.read((char *)&format.chunk, sizeof(ChunkHeader));

		// チャンクIDぁE"fmt " なめEbreak
		if(strncmp(format.chunk.id, "fmt ", 4) == 0) {
			break;
		}

		// それ以外ならスキチE�E
		file.seekg(format.chunk.size, std::ios_base::cur);
	}
	// チャンク本体�E読み込み
	assert(format.chunk.size <= sizeof(format.fmt));
	file.read((char *)&format.fmt, format.chunk.size);
	// Dataチャンクの読み込み
	ChunkHeader data;
	file.read((char *)&data, sizeof(data));
	// JUNKチャンクを検�Eした場吁E
	if(strncmp(data.id, "JUNK", 4) == 0) {
		// 読み取り位置をJUNKチャンクの終わりまで進める
		file.seekg(data.size, std::ios_base::cur);
		// 再読み込み
		file.read((char *)&data, sizeof(data));
	}

	if(strncmp(data.id, "data", 4) != 0) {
		assert(0);
	}

	// DataチャンクのチE�Eタ部刁E��み込み
    auto pBuffer = std::make_unique<char[]>(data.size);
    file.read(pBuffer.get(), data.size);

	// waveファイルを閉じる
	file.close();

	/*.読み込んだ音声チE�Eタをreturn
	*********************************************************/

	// returnするための音声チE�Eタ
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

    // 1. SourceReaderの作�E
    std::wstring wFilename = ConvertString(filename);
    hr = MFCreateSourceReaderFromURL(wFilename.c_str(), nullptr, &pSourceReader);
    assert(SUCCEEDED(hr));

    // 2. 出力形式をPCM�E�解凍後�E生データ�E�に設宁E
    Microsoft::WRL::ComPtr<IMFMediaType> pTargetMediaType;
    MFCreateMediaType(&pTargetMediaType);
    pTargetMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    pTargetMediaType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
    pSourceReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, pTargetMediaType.Get());

    // 3. 最終的な波形フォーマットを取征E
    Microsoft::WRL::ComPtr<IMFMediaType> pActualMediaType;
    pSourceReader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, &pActualMediaType);

    WAVEFORMATEX *pWfex;
    UINT32 wfexSize;
    MFCreateWaveFormatExFromMFMediaType(pActualMediaType.Get(), &pWfex, &wfexSize);

    // 4. 全てのサンプルを読み込んでバッファに格紁E
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

    // SoundData構造体に詰めて返す
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

    // 生�EするResourceの設宁E
    D3D12_RESOURCE_DESC resourceDesc{};
    resourceDesc.Width = width;
    resourceDesc.Height = height;
    resourceDesc.MipLevels = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.Format = format;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

    // ☁E��E��の持E��1: RenderTargetとして利用可能にする特殊なフラグ
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    // 利用するHeapの設宁E
    D3D12_HEAP_PROPERTIES heapProperties{};
    // ☁E��E��の持E��2: 当然VRAM上に作る (DEFAULT)
    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

    // クリア時�E色を設定（レンダーターゲチE��生�E時にはこれが忁E��です！E
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

    // --- 頂点座標�E定義 ---
    // 右面 (+X)
    vertices[0].position = {1.0f, 1.0f, 1.0f, 1.0f};
    vertices[1].position = {1.0f, 1.0f, -1.0f, 1.0f};
    vertices[2].position = {1.0f, -1.0f, 1.0f, 1.0f};
    vertices[3].position = {1.0f, -1.0f, -1.0f, 1.0f};
    // 左面 (-X)
    vertices[4].position = {-1.0f, 1.0f, -1.0f, 1.0f};
    vertices[5].position = {-1.0f, 1.0f, 1.0f, 1.0f};
    vertices[6].position = {-1.0f, -1.0f, -1.0f, 1.0f};
    vertices[7].position = {-1.0f, -1.0f, 1.0f, 1.0f};
    // 前面 (+Z)
    vertices[8].position = {-1.0f, 1.0f, 1.0f, 1.0f};
    vertices[9].position = {1.0f, 1.0f, 1.0f, 1.0f};
    vertices[10].position = {-1.0f, -1.0f, 1.0f, 1.0f};
    vertices[11].position = {1.0f, -1.0f, 1.0f, 1.0f};
    // 後面 (-Z)
    vertices[12].position = {1.0f, 1.0f, -1.0f, 1.0f};
    vertices[13].position = {-1.0f, 1.0f, -1.0f, 1.0f};
    vertices[14].position = {1.0f, -1.0f, -1.0f, 1.0f};
    vertices[15].position = {-1.0f, -1.0f, -1.0f, 1.0f};
    // 上面 (+Y)
    vertices[16].position = {-1.0f, 1.0f, -1.0f, 1.0f};
    vertices[17].position = {1.0f, 1.0f, -1.0f, 1.0f};
    vertices[18].position = {-1.0f, 1.0f, 1.0f, 1.0f};
    vertices[19].position = {1.0f, 1.0f, 1.0f, 1.0f};
    // 下面 (-Y)
    vertices[20].position = {-1.0f, -1.0f, 1.0f, 1.0f};
    vertices[21].position = {1.0f, -1.0f, 1.0f, 1.0f};
    vertices[22].position = {-1.0f, -1.0f, -1.0f, 1.0f};
    vertices[23].position = {1.0f, -1.0f, -1.0f, 1.0f};

    // --- インチE��クスの定義�E��E側を向く頁E��！E---
    // 吁E�� [0,1,2][2,1,3] のパターンで訁E6倁E
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


Animation LoadAnimationFile(const std::string& directoryPath, const std::string& filename) {
    Animation animation; // create animation
    Assimp::Importer importer;
    std::string filePath = directoryPath + "/" + filename;
    const aiScene* scene = importer.ReadFile(filePath.c_str(), 0);
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
            keyframe.value = {-keyAssimp.mValue.x, keyAssimp.mValue.y, keyAssimp.mValue.z}; // right-hand to left-hand
            nodeAnimation.translate.push_back(keyframe);
        }

        // Rotate
        for (uint32_t keyIndex = 0; keyIndex < nodeAnimationAssimp->mNumRotationKeys; ++keyIndex) {
            aiQuatKey& keyAssimp = nodeAnimationAssimp->mRotationKeys[keyIndex];
            KeyframeQuaternion keyframe;
            keyframe.time = float(keyAssimp.mTime / animationAssimp->mTicksPerSecond); // convert to seconds
            // right-hand to left-hand conversion for Quaternion
            keyframe.value = {keyAssimp.mValue.x, -keyAssimp.mValue.y, -keyAssimp.mValue.z, keyAssimp.mValue.w};
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

