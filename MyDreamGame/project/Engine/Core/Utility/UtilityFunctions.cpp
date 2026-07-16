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

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
#ifdef USE_IMGUI


	if(ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
		return true;
	}
#endif // USE_IMGUI

	// 繝｡繝・そ繝ｼ繧ｸ縺ｫ蠢懊§縺ｦ繧ｲ繝ｼ繝蝗ｺ譛峨・蜃ｦ逅・ｒ陦後≧
	switch(msg) {
		// 繧ｦ繧｣繝ｳ繝峨え縺檎ｴ螢翫＆繧後◆
	case WM_DESTROY:
		// OS縺ｫ蠢懊§縺ｦ縲√い繝励Μ蝗ｺ譛峨・邨ゆｺ・ｒ莨昴∴繧・
		PostQuitMessage(0);
		return 0;
	}

	// 讓呎ｺ悶・繝｡繝・そ繝ｼ繧ｸ蜃ｦ逅・ｒ陦後≧
	return DefWindowProc(hwnd, msg, wparam, lparam);
}

void Log(const std::string &message) {
	// 繝・ヰ繝・げ蜃ｺ蜉幢ｼ亥ｾ捺擂縺ｮ蜍穂ｽ懶ｼ・
	OutputDebugStringA(message.c_str());

	// 繝ｭ繧ｰ繝輔ぃ繧､繝ｫ繧剃ｸ蠎ｦ縺縺台ｽ懈・縺励※菴ｿ縺・屓縺呻ｼ医せ繝ｬ繝・ラ繧ｻ繝ｼ繝包ｼ・
	static std::once_flag s_logInitFlag;
	static std::ofstream s_logStream;
	static std::mutex s_logMutex;

	std::call_once(s_logInitFlag, []() {
		try {
			// logs 繝・ぅ繝ｬ繧ｯ繝医Μ繧剃ｽ懈・
			std::filesystem::create_directories("logs");

			// 迴ｾ蝨ｨ縺ｮ譎ょ綾繧堤ｧ貞腰菴阪↓荳ｸ繧√ｋ
			auto now = std::chrono::system_clock::now();
			auto nowSeconds = std::chrono::time_point_cast<std::chrono::seconds>(now);

			// 繝ｭ繝ｼ繧ｫ繝ｫ繧ｿ繧､繝繧ｾ繝ｼ繝ｳ縺ｫ螟画鋤縺励※繝輔か繝ｼ繝槭ャ繝茨ｼ亥・繧ｳ繝ｼ繝峨→蜷後§譖ｸ蠑上ｒ菴ｿ逕ｨ・・
			std::chrono::zoned_time localTime{ std::chrono::current_zone(), nowSeconds };
			std::string dateString = std::format("{:%Y%d_%H%M%S}", localTime);

			// 繝輔ぃ繧､繝ｫ繝代せ繧剃ｽ懈・縺励※ open・郁ｿｽ險倥Δ繝ｼ繝会ｼ・
			std::string logFilePath = std::string("logs/") + dateString + ".log";
			s_logStream.open(logFilePath, std::ios::app | std::ios::binary);
		} catch(...) {
			// 萓句､悶・辟｡隕悶＠縺ｦ繝・ヰ繝・げ蜃ｺ蜉帙・縺ｿ陦後≧・医Ο繧ｰ螟ｱ謨励＠縺ｦ繧ゅい繝励Μ縺梧ｭ｢縺ｾ繧峨↑縺・ｈ縺・↓縺吶ｋ・・
		}
				   });

				   // 螳滄圀縺ｮ譖ｸ縺崎ｾｼ縺ｿ
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
	// 譎ょ綾繧貞叙蠕励＠縺ｦ縲∵凾蛻ｻ繧貞錐蜑阪↓蜈･繧後◆繝輔ぃ繧､繝ｫ繧剃ｽ懈・縲・umps繝・ぅ繝ｬ繧ｯ繝医Μ莉･荳九↓蜃ｺ蜉・
	SYSTEMTIME time;
	GetLocalTime(&time);
	wchar_t filePath[MAX_PATH] = { 0 };

	// 繝・ぅ繝ｬ繧ｯ繝医Μ菴懈・・亥､ｱ謨励＠縺ｦ繧らｶ夊｡鯉ｼ・
	if(!CreateDirectoryW(L"./Dumps", nullptr)) {
		DWORD err = GetLastError();
		if(err != ERROR_ALREADY_EXISTS) {
			Log(std::format("CreateDirectory failed, err:{}\n", err));
		}
	}

	// 繝輔ぃ繧､繝ｫ蜷搾ｼ育ｧ貞腰菴搾ｼ・
	StringCchPrintfW(filePath, MAX_PATH, L"./Dumps/%04d-%02d-%02d_%02d%02d%02d.dmp",
					 time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);

				 // 繝ｭ繧ｰ縺ｫ繝代せ繧貞・蜉・
	Log(std::format("ExportDump: target path: {}\n", ConvertString(filePath)));

	// 繝輔ぃ繧､繝ｫ菴懈・
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

	// processId(exeID)縺ｨ繧ｯ繝ｩ繝・す繝･(萓句､・縺ｮ逋ｺ逕溘＠縺殳hreadID繧貞叙蠕・
	DWORD processId = GetCurrentProcessId();
	DWORD threadId = GetCurrentThreadId();

	// 險ｭ螳壽ュ蝣ｱ繧貞・蜉・
	MINIDUMP_EXCEPTION_INFORMATION minidumpInformation{};
	minidumpInformation.ThreadId = threadId;
	minidumpInformation.ExceptionPointers = exception;
	minidumpInformation.ClientPointers = TRUE;

	// Dump繧貞・蜉帙らｵ先棡繧偵Ο繧ｰ縺ｫ谿九☆
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

	// 縺ｻ縺九↓髢｢騾｣莉倥￠繧峨ｌ縺ｦ縺・ｋSEH萓句､悶ワ繝ｳ繝峨Λ縺後≠繧後・螳溯｡後る壼ｸｸ繝励Ο繧ｻ繧ｹ繧堤ｵゆｺ・☆繧九・
	return EXCEPTION_EXECUTE_HANDLER;
}

IDxcBlob *CompileShader(
	// Compiler縺吶ｋShader繝輔ぃ繧､繝ｫ縺ｸ縺ｮ繝代せ
	const std::wstring &filePath,
	// Compiler縺ｫ菴ｿ逕ｨ縺吶ｋProfile
	const wchar_t *profile,
	// 蛻晄悄蛹悶〒逕滓・縺励◆繧ゅ・繧・縺､
	IDxcUtils *dxcUtils,
	IDxcCompiler3 *dxcCompiler,
	IDxcIncludeHandler *includeHandler) {

	/*********************************************************
	*1. hlsl繝輔ぃ繧､繝ｫ繧定ｪｭ繧
	*********************************************************/

	// 縺薙ｌ縺九ｉ繧ｷ繧ｧ繝ｼ繝繝ｼ繧偵さ繝ｳ繝代う繝ｫ縺吶ｋ譌ｨ繧偵Ο繧ｰ縺ｫ蜃ｺ縺・
	Log(ConvertString(std::format(L"Begin CompileShader, path:{},profile:{}\n", filePath, profile)));
	// hlsl繝輔ぃ繧､繝ｫ繧定ｪｭ繧
	IDxcBlobEncoding *shaderSource = nullptr;
	HRESULT hr = dxcUtils->LoadFile(filePath.c_str(), nullptr, &shaderSource);
	if (FAILED(hr)) {
		wchar_t buf[MAX_PATH];
		GetCurrentDirectoryW(MAX_PATH, buf);
		Log(ConvertString(std::format(L"Failed to load shader file: {}. hr: {:08X}, CWD: {}\n", filePath, hr, buf)));
	}
	// 縺ゅ″繧峨ａ縺ｪ縺九▲縺溘ｉ豁｢繧√ｋ
	assert(SUCCEEDED(hr));
	// 隱ｭ縺ｿ霎ｼ繧薙□繝輔ぃ繧､繝ｫ縺ｮ蜀・ｮｹ繧定ｨｭ螳壹☆繧・
	DxcBuffer shaderSourceBuffer;
	shaderSourceBuffer.Ptr = shaderSource->GetBufferPointer();
	shaderSourceBuffer.Size = shaderSource->GetBufferSize();
	shaderSourceBuffer.Encoding = DXC_CP_UTF8;// UTF8縺ｮ譁・ｭ励さ繝ｼ繝峨〒縺ゅｋ縺薙→繧帝夂衍

	/*********************************************************
	*2.Compile縺吶ｋ
	*********************************************************/

	LPCWSTR arguments[] = {
        filePath.c_str(),         // 繧ｳ繝ｳ繝代う繝ｫ蟇ｾ雎｡縺ｮhlsl繝輔ぃ繧､繝ｫ蜷・
        L"-E", L"main",           // 繧ｨ繝ｳ繝医Μ繝ｼ繝昴う繝ｳ繝医・謖・ｮ壹ょ渕譛ｬ逧・↓main莉･螟悶↓縺ｯ縺励↑縺・
        L"-T", profile,           // ShaderProfile縺ｮ險ｭ螳・
        L"-Zi", L"-Qembed_debug", // 繝・ヰ繝・げ逕ｨ縺ｮ諠・ｱ繧貞沂繧∬ｾｼ繧
        L"-Od",                   // 譛驕ｩ蛹悶ｒ螟悶＠縺ｦ縺翫￥
        L"-Zpr",                  // 繝｡繝｢繝ｪ繝ｬ繧､繧｢繧ｦ繝医・陦悟━蜈・
        L"-HV", L"2021",          // 笘・縺薙ｌ繧定ｿｽ蜉・・HLSL2021繝ｫ繝ｼ繝ｫ繧帝←逕ｨ縺励※C++縺ｨ蜷後§蝙句錐繧剃ｽｿ縺医ｋ繧医≧縺ｫ縺吶ｋ
    };
	// 螳滄圀縺ｫShader繧偵さ繝ｳ繝代う繝ｫ縺吶ｋ
	IDxcResult *shaderResult = nullptr;
	hr = dxcCompiler->Compile(
		&shaderSourceBuffer,
		arguments,
		_countof(arguments),
		includeHandler,
		IID_PPV_ARGS(&shaderResult)
	);
	// 繧ｳ繝ｳ繝代う繝ｩ繧ｨ繝ｩ繝ｼ縺ｧ縺ｯ縺ｪ縺重xc縺瑚ｵｷ蜍輔〒縺阪↑縺・↑縺ｩ縺ｮ閾ｴ蜻ｽ逧・↑繧ｨ繝ｩ繝ｼ
	assert(SUCCEEDED(hr));
/*********************************************************
	*3.隴ｦ蜻翫・繧ｨ繝ｩ繝ｼ縺悟・縺ｦ縺・↑縺・°遒ｺ隱・
	*********************************************************/

	IDxcBlobUtf8 *shaderError = nullptr;
	shaderResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&shaderError), nullptr);

	// shaderError縺御ｽ懊ｉ繧後※縺・※縲√°縺､荳ｭ霄ｫ縺ｮ譁・ｭ怜・縺ｮ髟ｷ縺輔′0縺ｧ縺ｯ縺ｪ縺・ｴ蜷医□縺代お繝ｩ繝ｼ縺ｨ縺ｿ縺ｪ縺・
	if(shaderError != nullptr && shaderError->GetStringLength() != 0) {
		Log(shaderError->GetStringPointer());
		// 隴ｦ蜻翫・繧ｨ繝ｩ繝ｼ邨ｶ蟇ｾ繝繝｡
		assert(false);
	}
	/*********************************************************
	*4.Compile邨先棡繧貞女縺大叙縺｣縺ｦ霑斐☆
	*********************************************************/

	// 繧ｳ繝ｳ繝代う繝ｫ邨先棡縺九ｉ螳溯｡檎畑縺ｮ繝舌う繝翫Μ驛ｨ蛻・ｒ蜿門ｾ・
	IDxcBlob *shaderBlob = nullptr;
	hr = shaderResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
	assert(SUCCEEDED(hr));
	// 謌仙粥謚ｼ縺励◆繝ｭ繧ｰ繧貞・縺・
	Log(ConvertString(std::format(L"Compile Succeeded, path:{},profile:{}\n", filePath, profile)));
	// 繧ゅ≧菴ｿ繧上↑縺・Μ繧ｽ繝ｼ繧ｹ繧帝幕謾ｾ
	shaderSource->Release();
	shaderResult->Release();
	// 螳溯｡檎畑縺ｮ繝舌う繝翫Μ繧定ｿ泌唆
	return shaderBlob;
}

Microsoft::WRL::ComPtr<ID3D12Resource> CreateBufferResource(Microsoft::WRL::ComPtr<ID3D12Device> device, size_t sizeInBytes) {
	assert(device != nullptr); // 螳牙・繝√ぉ繝・け

	// 繧｢繝・・繝ｭ繝ｼ繝臥畑縺ｮ繝偵・繝励・險ｭ螳夲ｼ・PU縺九ｉGPU縺ｫ繝・・繧ｿ繧帝√ｋ逕ｨ・・
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

	// 繝舌ャ繝輔ぃ繝ｪ繧ｽ繝ｼ繧ｹ縺ｮ險ｭ螳・
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Width = sizeInBytes;
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	// 螳滄圀縺ｫ繝ｪ繧ｽ繝ｼ繧ｹ・医ヰ繝・ヵ繧｡・峨ｒ菴懈・
	Microsoft::WRL::ComPtr<ID3D12Resource> resource = nullptr;
	HRESULT hr = device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_COMMON, // 蛻晄悄迥ｶ諷具ｼ郁ｪｭ縺ｿ蜿悶ｊ逕ｨ・・
		nullptr,
		IID_PPV_ARGS(&resource)
	);
	if (FAILED(hr)) {
		throw std::runtime_error("CreateBufferResource failed! VRAM might be full or arguments invalid.");
	}

	return resource; // 菴懊▲縺溘ヰ繝・ヵ繧｡繧定ｿ斐☆・・
}

// DescriptorHeap縺ｮ菴懈・髢｢謨ｰ
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

// Texture繝・・繧ｿ繧定ｪｭ繧
DirectX::ScratchImage LoadTexture(const std::string &filePath) {
    // 繝輔ぃ繧､繝ｫ繝代せ遒ｺ隱咲畑縺ｮ繝ｭ繧ｰ
    OutputDebugStringA(("LoadTexture: " + filePath + "\n").c_str());

    DirectX::ScratchImage image{};
    std::wstring filePathW = ConvertString(filePath);
    HRESULT hr;

    // 笘・ｳ・侭縺ｮ謖・､ｺ1・咼DS繝輔ぃ繧､繝ｫ縺ｫ蟇ｾ蠢懊☆繧・
    if (filePathW.ends_with(L".dds")) {
        // .dds縺ｧ邨ゅｏ縺｣縺ｦ縺・◆繧吋DS縺ｨ縺励※隱ｭ縺ｿ霎ｼ繧縲ＴRGB諠・ｱ縺悟性縺ｾ繧後※縺・ｋ縺ｮ縺ｧ繝輔Λ繧ｰ縺ｯNONE
        hr = DirectX::LoadFromDDSFile(filePathW.c_str(), DirectX::DDS_FLAGS_NONE, nullptr, image);
    } else {
        // 縺昴ｌ莉･螟悶・蠕捺擂騾壹ｊWIC・・NG繧ЙPG縺ｪ縺ｩ・峨→縺励※隱ｭ縺ｿ霎ｼ繧
        hr = DirectX::LoadFromWICFile(filePathW.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
    }
    if (FAILED(hr)) {
        throw std::runtime_error("LoadTexture failed to load file: " + filePath);
    }

    // 笘・ｳ・侭縺ｮ謖・､ｺ2・壼悸邵ｮ繝輔か繝ｼ繝槭ャ繝医°蛻､螳壹＠縺ｦ繝溘ャ繝励・繝・・逕滓・繧貞・縺代ｋ
    DirectX::ScratchImage mipImages{};
    if (DirectX::IsCompressed(image.GetMetadata().format)) {
        // 蝨ｧ邵ｮ繝輔か繝ｼ繝槭ャ繝医↑繧峨◎縺ｮ縺ｾ縺ｾ菴ｿ縺・ｼ・irectXTex縺檎峩謗･縺ｮ繝溘ャ繝励・繝・・逕滓・縺ｫ髱槫ｯｾ蠢懊↑縺溘ａ・・
        mipImages = std::move(image);
    } else {
        // 髱槫悸邵ｮ縺ｪ繧峨Α繝・・繝槭ャ繝励ｒ菴懈・縺吶ｋ
        hr = DirectX::GenerateMipMaps(
            image.GetImages(), image.GetImageCount(), image.GetMetadata(),
            DirectX::TEX_FILTER_SRGB, 4, mipImages); // 隨ｬ5蠑墓焚縺ｮ 0(MAX) 繧・4 縺ｪ縺ｩ莉ｻ諢上↓螟画峩蜿ｯ閭ｽ
        assert(SUCCEEDED(hr));
    }

    return mipImages;
}

// DirectX12縺ｮTextureResource繧剃ｽ懊ｋ
Microsoft::WRL::ComPtr<ID3D12Resource> CreateTextureResource(Microsoft::WRL::ComPtr<ID3D12Device> device, const DirectX::TexMetadata &metadata) {
	// metadata繧偵ｂ縺ｨ縺ｫResource縺ｮ險ｭ螳・
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = UINT(metadata.width);// 讓ｪ蟷・
	resourceDesc.Height = UINT(metadata.height);// 鬮倥＆
	resourceDesc.MipLevels = UINT(metadata.mipLevels);// mipmap縺ｮ謨ｰ
	resourceDesc.DepthOrArraySize = UINT(metadata.arraySize);// 螂･陦後″ ro 驟榊・Texture縺ｮ驟榊・謨ｰ
	resourceDesc.Format = metadata.format;// Texture縺ｮFormat
	resourceDesc.SampleDesc.Count = 1;// 繧ｵ繝ｳ繝励Μ繝ｳ繧ｰ繧ｫ繧ｦ繝ｳ繝・
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION(metadata.dimension);// Texture縺ｮ谺｡蜈・焚縲・谺｡蜈・

	// 蛻ｩ逕ｨ縺吶ｋHeap縺ｮ險ｭ螳・
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;// 邏ｰ縺九＞險ｭ螳壹ｒ陦後≧

	// Resource縺ｮ逕滓・
	Microsoft::WRL::ComPtr<ID3D12Resource> resource = nullptr;
	HRESULT hr = device->CreateCommittedResource(
		&heapProperties,// Heap縺ｮ險ｭ螳・
		D3D12_HEAP_FLAG_NONE,// Heap縺ｮ迚ｹ谿翫↑險ｭ螳壹ゆｻ雁屓縺ｯ縺ｪ縺・
		&resourceDesc,// Resource縺ｮ險ｭ螳・
		D3D12_RESOURCE_STATE_COPY_DEST,// 蛻晏屓縺ｮResourceState縲・
		nullptr,//Clear譛驕ｩ蛟､縲ゆｻ雁屓縺ｯ菴ｿ繧上↑縺・
		IID_PPV_ARGS(&resource)
	);
	if (FAILED(hr)) {
		throw std::runtime_error("CreateTextureResource failed to create committed resource.");
	}
	return resource;
}

// 謌ｻ繧雁､繧堤ｴ謳阪＠縺ｦ縺ｯ縺ｪ繧峨↑縺・・縺ｧ縺薙ｌ繧剃ｻ倥￠繧・
[[nodiscard]]
// TextureResouorce縺ｫ繝・・繧ｿ繧定ｻ｢騾√☆繧・
Microsoft::WRL::ComPtr<ID3D12Resource> UploadTextureData(Microsoft::WRL::ComPtr<ID3D12Resource> texture, const DirectX::ScratchImage &mipImages, Microsoft::WRL::ComPtr<ID3D12Device> device, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList) {
	std::vector<D3D12_SUBRESOURCE_DATA>subresources;
	// 隱ｭ縺ｿ霎ｼ繧薙□繝・・繧ｿ縺九ｉDirectX12逕ｨ縺ｮSubresource縺ｮ驟榊・繧剃ｽ懈・
	DirectX::PrepareUpload(device.Get(), mipImages.GetImages(), mipImages.GetImageCount(), mipImages.GetMetadata(), subresources);
	// IntermediateResource縺ｫ蠢・ｦ√↑繧ｵ繧､繧ｺ繧定ｨ育ｮ励☆繧・
	uint64_t intermediateSize = GetRequiredIntermediateSize(texture.Get(), 0, UINT(subresources.size()));
	// 險育ｮ励＠縺溘し繧､繧ｺ縺ｧIntermediateResource繧剃ｽ懊ｋ
	Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource = CreateBufferResource(device, intermediateSize);
	// 繝・・繧ｿ霆｢騾√ｒ繧ｳ繝槭Φ繝峨↓遨阪・
	UpdateSubresources(commandList.Get(), texture.Get(), intermediateResource.Get(), 0, 0, UINT(subresources.size()), subresources.data());
	// Teture縺ｸ縺ｮ霆｢騾∝ｾ後・蛻ｩ逕ｨ縺ｧ縺阪ｋ繧医≧縲．3D12_RESOURCE_STATE_COPY_DEST縺九ｉD3D12_RESOURCE_STATE_GENERIC_READ縺ｸResourceState繧貞､画峩縺吶ｋ
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
	// 逕滓・縺吶ｋResource縺ｮ險ｭ螳・
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = width;
	resourceDesc.Height = height;
	resourceDesc.MipLevels = 1;
	resourceDesc.DepthOrArraySize = 1;// 螂･陦後″
	resourceDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;// 2谺｡蜈・
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	// 蛻ｩ逕ｨ縺吶ｋHeap縺ｮ險ｭ螳・
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

	// 豺ｱ螻､蛟､縺ｮ繧ｯ繝ｪ繧｢險ｭ螳・
	D3D12_CLEAR_VALUE depthClearValue{};
	depthClearValue.DepthStencil.Depth = 1.0f;
	depthClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;// 繝輔か繝ｼ繝槭ャ繝医ｒResource縺ｨ蜷医ｏ縺帙ｋ

	// Resource縺ｮ險ｭ螳・
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

// DescriptorHandle繧貞叙蠕励☆繧・CPU)
D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap, uint32_t descriptorSize, uint32_t index) {
	D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
	handleCPU.ptr += (descriptorSize * index);
	return handleCPU;
}

// DescriptorHandle繧貞叙蠕励☆繧・GPU)
D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap, uint32_t descriptorSize, uint32_t index) {
	D3D12_GPU_DESCRIPTOR_HANDLE handleGPU = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
	handleGPU.ptr += (descriptorSize * index);
	return handleGPU;
}



void CreateSphereMesh(std::vector<VertexData> &vertices, std::vector<uint32_t> &indices, float radius, int latDiv, int lonDiv) {
	// 邱ｯ蠎ｦ縺ｮ蛻・牡謨ｰ: 荳翫°繧我ｸ九∈菴墓ｮｵ縺ｫ蛻・￠繧九°
	// 邨悟ｺｦ縺ｮ蛻・牡謨ｰ: 讓ｪ縺ｫ菴募・蜑ｲ縺吶ｋ縺具ｼ郁ｵ､驕薙・霈ｪ蛻・ｊ縺ｿ縺溘＞縺ｪ繧､繝｡繝ｼ繧ｸ・・

	// 鬆らせ縺ｮ逕滓・・育ｷｯ蠎ｦ譁ｹ蜷代↓繝ｫ繝ｼ繝暦ｼ・
	for(int lat = 0; lat <= latDiv; ++lat) {
		float theta = lat * float(M_PI) / float(latDiv); // 邱ｯ蠎ｦ縺ｮ隗貞ｺｦ・・ ~ ﾏ・・
		float sinTheta = sinf(theta);
		float cosTheta = cosf(theta);

		// 邨悟ｺｦ譁ｹ蜷代↓繝ｫ繝ｼ繝・
		for(int lon = 0; lon <= lonDiv; ++lon) {
			float phi = lon * 2.0f * float(M_PI) / float(lonDiv); // 邨悟ｺｦ縺ｮ隗貞ｺｦ・・ ~ 2ﾏ・・
			float sinPhi = sinf(phi);
			float cosPhi = cosf(phi);

			// 逅・・x, y, z蠎ｧ讓吶ｒ豎ゅａ繧・
			float x = cosPhi * sinTheta;
			float y = cosTheta;
			float z = sinPhi * sinTheta;

			// 鬆らせ繝・・繧ｿ繧剃ｽ懈・
			VertexData v{};
			v.position = { radius * x, radius * y, radius * z, 1.0f }; // 逅・・陦ｨ髱｢荳翫・轤ｹ
			v.normal = {v.position.x / radius, v.position.y / radius, v.position.z / radius};
            v.texcoord = { (float)lon / lonDiv, (float)lat / latDiv };
            v.color = {1.0f, 1.0f, 1.0f, 1.0f};
            vertices.push_back(v); // 鬆らせ繝ｪ繧ｹ繝医↓霑ｽ蜉
		}
	}
	// 荳芽ｧ貞ｽ｢繧､繝ｳ繝・ャ繧ｯ繧ｹ縺ｮ逕滓・・磯らせ繧偵▽縺ｪ縺撰ｼ・
	for(int lat = 0; lat < latDiv; ++lat) {
		for(int lon = 0; lon < lonDiv; ++lon) {
			// 迴ｾ蝨ｨ縺ｮ陦後・蛻励°繧蛾らせ縺ｮ逡ｪ蜿ｷ繧定ｨ育ｮ・
			int first = lat * (lonDiv + 1) + lon;
			int second = first + lonDiv + 1;

			// 莠後▽縺ｮ荳芽ｧ貞ｽ｢繧剃ｽｿ縺｣縺ｦ蝗幄ｧ貞ｽ｢繧貞沂繧√ｋ
			indices.push_back(first);         // 蟾ｦ荳・
			indices.push_back(first + 1);     // 蜿ｳ荳・
			indices.push_back(second);        // 蟾ｦ荳・

			indices.push_back(second);        // 蟾ｦ荳・
			indices.push_back(first + 1);     // 蜿ｳ荳・
			indices.push_back(second + 1);    // 蜿ｳ荳・
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

    // 1. 繝輔ぃ繧､繝ｫ縺ｮ隱ｭ縺ｿ霎ｼ縺ｿ
    // 雉・侭縺ｫ縺ゅｋ騾壹ｊ縲∽ｸ芽ｧ貞ｽ｢蛹悶∝ｷｻ縺埼・渚霆｢縲ゞV蜿崎ｻ｢繧呈欠螳・
    const aiScene *scene = importer.ReadFile(filePath.c_str(),
                                             // 1. 縺吶∋縺ｦ縺ｮ髱｢繧剃ｸ芽ｧ貞ｽ｢縺ｫ螟画鋤・・irectX縺檎炊隗｣縺ｧ縺阪ｋ蠖｢蠑上↓縺吶ｋ・・
                                             aiProcess_Triangulate |
                                                 // 2. V霆ｸ繧貞渚霆｢・・lTF縺ｪ縺ｩ縺ｮ蟾ｦ荳句次轤ｹ繧偵．irectX讓呎ｺ悶・蟾ｦ荳雁次轤ｹ縺ｫ蜷医ｏ縺帙ｋ・・
                                                 aiProcess_FlipUVs |
                                                 // 3. 蜿ｳ謇狗ｳｻ縺九ｉ蟾ｦ謇狗ｳｻ縺ｸ螟画鋤・郁ｻｸ縺ｮ蜿崎ｻ｢繧・ｷｻ縺埼・・隱ｿ謨ｴ繧偵そ繝・ヨ縺ｧ陦後≧・・
                                                 aiProcess_ConvertToLeftHanded |
                                                 // 4. 法線がない場合に滑らかな法線を生成（ライティング計算に必要）
                                                 aiProcess_GenSmoothNormals |
                                                 // グローバルスケールを適用
                                                 aiProcess_GlobalScale);

    // 繝｡繝・す繝･縺後↑縺・ｴ蜷医・繧ｨ繝ｩ繝ｼ
    assert(scene && scene->HasMeshes());

    // 2. 繝｡繝・す繝･縺ｮ隗｣譫撰ｼ郁ｳ・侭縺ｫ蝓ｺ縺･縺阪∝・繝｡繝・す繝･繧偵Ν繝ｼ繝暦ｼ・
    for (uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
        aiMesh *mesh = scene->mMeshes[meshIndex];

        // MultiMesh/MultiMaterial対応のため、頂点の開始位置を記録しておく
        uint32_t vertexOffset = static_cast<uint32_t>(modelData.vertices.size());

        // 豕慕ｷ壹→Texcoord縺後↑縺・Γ繝・す繝･縺ｯ莉雁屓縺ｯ髱槫ｯｾ蠢懶ｼ郁ｳ・侭縺ｮassert・・
        assert(mesh->HasNormals());
        assert(mesh->HasTextureCoords(0));

        // 鬆らせ繝・・繧ｿ縺ｮ隗｣譫・
        for (uint32_t vIndex = 0; vIndex < mesh->mNumVertices; ++vIndex) {
            aiVector3D &position = mesh->mVertices[vIndex];
            aiVector3D &normal = mesh->mNormals[vIndex];
            aiVector3D &texcoord = mesh->mTextureCoords[0][vIndex];

            VertexData vertex;
            vertex.position = {position.x, position.y, position.z, 1.0f};
            vertex.normal = {normal.x, normal.y, normal.z};
            vertex.texcoord = {texcoord.x, texcoord.y};

            // 蟾ｦ謇狗ｳｻ縺ｸ縺ｮ螟画鋤・郁ｳ・侭縺ｮ騾壹ｊ縲々繧貞渚霆｢・・
            vertex.position = {position.x, position.y, position.z, 1.0f};
            vertex.normal = {normal.x, normal.y, normal.z};
            vertex.texcoord = {texcoord.x, texcoord.y};
            vertex.color = {1.0f, 1.0f, 1.0f, 1.0f};
            modelData.vertices.push_back(vertex);
        }

        // 繧､繝ｳ繝・ャ繧ｯ繧ｹ・・ace・峨・隗｣譫撰ｼ郁ｳ・侭・唔ndexed謠冗判縺ｫ蟇ｾ蠢懊＆縺帙ｋ・・
        
        // Bone解析
        for (uint32_t boneIndex = 0; boneIndex < mesh->mNumBones; ++boneIndex) {
            aiBone* bone = mesh->mBones[boneIndex];
            std::string jointName = bone->mName.C_Str();
            JointWeightData& weightData = modelData.skinClusterData[jointName];

            aiMatrix4x4 bindPoseMatrixAssimp = bone->mOffsetMatrix.Inverse(); // BindPoseMatrixに戻す
            aiVector3D scale, translate;
            aiQuaternion rotate;
            bindPoseMatrixAssimp.Decompose(scale, rotate, translate); // 成分を抽出
            // 左手系のBindPoseMatrixを作る (AssimpのaiProcess_ConvertToLeftHandedにより既に左手系に変換されているため、反転は不要)
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

        for (uint32_t faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex) {
            aiFace &face = mesh->mFaces[faceIndex];
            assert(face.mNumIndices == 3); // 荳芽ｧ貞ｽ｢縺ｮ縺ｿ繧ｵ繝昴・繝・

            for (uint32_t element = 0; element < face.mNumIndices; ++element) {
                uint32_t vertexIndex = face.mIndices[element];
                modelData.indices.push_back(vertexIndex + vertexOffset); // vertexOffsetを加算する
            }
        }
    }

    // 3. 繝槭ユ繝ｪ繧｢繝ｫ縺ｮ隗｣譫撰ｼ郁ｳ・侭縺ｫ蝓ｺ縺･縺阪．iffuse繝・け繧ｹ繝√Ε繧貞叙蠕暦ｼ・
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
	MaterialData materialData;// 讒狗ｯ峨☆繧貴aterialData
	std::string line;//縲繝輔ぃ繧､繝ｫ縺九ｉ隱ｭ繧薙□1陦檎岼繧呈ｼ邏阪☆繧・
	std::ifstream file(directoryPath + "/" + filename);// 繝輔ぃ繧､繝ｫ繧帝幕縺・
	assert(file.is_open());// 髢九￠縺ｪ縺九▲縺溘ｉ豁｢繧√ｋ

	while(std::getline(file, line)) {
		std::string identifier;
		std::istringstream s(line);
		s >> identifier;

		// identifier縺ｫ蠢懊§縺溷・逅・
		if(identifier == "map_Kd") {
			std::string textureFilename;
			s >> textureFilename;
			// 騾｣邨舌＠縺ｦ繝輔ぃ繧､繝ｫ繝代せ縺ｫ縺吶ｋ
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

	// 繝輔ぃ繧､繝ｫ蜈･蜉帙せ繝医Μ繝ｼ繝縺ｮ繧､繝ｳ繧ｹ繧ｿ繝ｳ繧ｹ
	std::ifstream file;
	// .wav繝輔ぃ繧､繝ｫ繧偵ヰ繧､繝翫Μ繝｢繝ｼ繝峨〒髢九￥
	file.open(filename, std::ios_base::binary);
	// 繝輔ぃ繧､繝ｫ繧ｪ繝ｼ繝励Φ螟ｱ謨励ｒ讀懷・縺吶ｋ
	assert(file.is_open());

	/*.wav繝・・繧ｿ隱ｭ縺ｿ霎ｼ縺ｿ
	*********************************************************/

	// RIFF繝倥ャ繝繝ｼ縺ｮ隱ｭ縺ｿ霎ｼ縺ｿ
	RiffHeader riff;
	file.read((char *)&riff, sizeof(riff));
	OutputDebugStringA(std::format("Read RIFF ID: {}\n", std::string(riff.chunk.id, 4)).c_str());
	// 繧ｿ繧､繝励′RIFF縺九メ繧ｧ繝・け
	if(strncmp(riff.chunk.id, "RIFF", 4) != 0) {
		assert(0);
	}
	// 繧ｿ繧､繝励′WAVE縺九メ繧ｧ繝・け
	if(strncmp(riff.type, "WAVE", 4) != 0) {
		assert(0);
	}

	// Format繝√Ε繝ｳ繧ｯ隱ｭ縺ｿ霎ｼ縺ｿ
	FormatChunk format = {};
	// fmt繝√Ε繝ｳ繧ｯ繧呈爾縺吶Ν繝ｼ繝・
	while(true) {
		// 繝√Ε繝ｳ繧ｯ繝倥ャ繝繝ｼ繧定ｪｭ繧
		file.read((char *)&format.chunk, sizeof(ChunkHeader));

		// 繝√Ε繝ｳ繧ｯID縺・"fmt " 縺ｪ繧・break
		if(strncmp(format.chunk.id, "fmt ", 4) == 0) {
			break;
		}

		// 縺昴ｌ莉･螟悶↑繧峨せ繧ｭ繝・・
		file.seekg(format.chunk.size, std::ios_base::cur);
	}
	// 繝√Ε繝ｳ繧ｯ譛ｬ菴薙・隱ｭ縺ｿ霎ｼ縺ｿ
	assert(format.chunk.size <= sizeof(format.fmt));
	file.read((char *)&format.fmt, format.chunk.size);
	// Data繝√Ε繝ｳ繧ｯ縺ｮ隱ｭ縺ｿ霎ｼ縺ｿ
	ChunkHeader data;
	file.read((char *)&data, sizeof(data));
	// JUNK繝√Ε繝ｳ繧ｯ繧呈､懷・縺励◆蝣ｴ蜷・
	if(strncmp(data.id, "JUNK", 4) == 0) {
		// 隱ｭ縺ｿ蜿悶ｊ菴咲ｽｮ繧谷UNK繝√Ε繝ｳ繧ｯ縺ｮ邨ゅｏ繧翫∪縺ｧ騾ｲ繧√ｋ
		file.seekg(data.size, std::ios_base::cur);
		// 蜀崎ｪｭ縺ｿ霎ｼ縺ｿ
		file.read((char *)&data, sizeof(data));
	}

	if(strncmp(data.id, "data", 4) != 0) {
		assert(0);
	}

	// Data繝√Ε繝ｳ繧ｯ縺ｮ繝・・繧ｿ驛ｨ蛻・ｪｭ縺ｿ霎ｼ縺ｿ
    auto pBuffer = std::make_unique<char[]>(data.size);
    file.read(pBuffer.get(), data.size);

	// wave繝輔ぃ繧､繝ｫ繧帝哩縺倥ｋ
	file.close();

	/*.隱ｭ縺ｿ霎ｼ繧薙□髻ｳ螢ｰ繝・・繧ｿ繧池eturn
	*********************************************************/

	// return縺吶ｋ縺溘ａ縺ｮ髻ｳ螢ｰ繝・・繧ｿ
	SoundData soundData = {};

	soundData.wfex = format.fmt;
    soundData.pBuffer.reset(reinterpret_cast<BYTE *>(pBuffer.release()));
	soundData.bufferSize = data.size;

	return soundData;

}

void SoundUnload(SoundData *soundData) {
	// 繝舌ャ繝輔ぃ縺ｮ繝｡繝｢繝ｪ繧定ｧ｣謾ｾ
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

    // 1. SourceReader縺ｮ菴懈・
    std::wstring wFilename = ConvertString(filename);
    hr = MFCreateSourceReaderFromURL(wFilename.c_str(), nullptr, &pSourceReader);
    assert(SUCCEEDED(hr));

    // 2. 蜃ｺ蜉帛ｽ｢蠑上ｒPCM・郁ｧ｣蜃榊ｾ後・逕溘ョ繝ｼ繧ｿ・峨↓險ｭ螳・
    Microsoft::WRL::ComPtr<IMFMediaType> pTargetMediaType;
    MFCreateMediaType(&pTargetMediaType);
    pTargetMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    pTargetMediaType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
    pSourceReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, pTargetMediaType.Get());

    // 3. 譛邨ら噪縺ｪ豕｢蠖｢繝輔か繝ｼ繝槭ャ繝医ｒ蜿門ｾ・
    Microsoft::WRL::ComPtr<IMFMediaType> pActualMediaType;
    pSourceReader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, &pActualMediaType);

    WAVEFORMATEX *pWfex;
    UINT32 wfexSize;
    MFCreateWaveFormatExFromMFMediaType(pActualMediaType.Get(), &pWfex, &wfexSize);

    // 4. 蜈ｨ縺ｦ縺ｮ繧ｵ繝ｳ繝励Ν繧定ｪｭ縺ｿ霎ｼ繧薙〒繝舌ャ繝輔ぃ縺ｫ譬ｼ邏・
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

    CoTaskMemFree(pWfex); // MF縺檎函謌舌＠縺溘ヵ繧ｩ繝ｼ繝槭ャ繝域ｧ矩菴薙ｒ隗｣謾ｾ
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

    // 逕滓・縺吶ｋResource縺ｮ險ｭ螳・
    D3D12_RESOURCE_DESC resourceDesc{};
    resourceDesc.Width = width;
    resourceDesc.Height = height;
    resourceDesc.MipLevels = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.Format = format;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

    // 笘・ｳ・侭縺ｮ謖・､ｺ1: RenderTarget縺ｨ縺励※蛻ｩ逕ｨ蜿ｯ閭ｽ縺ｫ縺吶ｋ迚ｹ谿翫↑繝輔Λ繧ｰ
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    // 蛻ｩ逕ｨ縺吶ｋHeap縺ｮ險ｭ螳・
    D3D12_HEAP_PROPERTIES heapProperties{};
    // 笘・ｳ・侭縺ｮ謖・､ｺ2: 蠖鍋┯VRAM荳翫↓菴懊ｋ (DEFAULT)
    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

    // 繧ｯ繝ｪ繧｢譎ゅ・濶ｲ繧定ｨｭ螳夲ｼ医Ξ繝ｳ繝繝ｼ繧ｿ繝ｼ繧ｲ繝・ヨ逕滓・譎ゅ↓縺ｯ縺薙ｌ縺悟ｿ・ｦ√〒縺呻ｼ・
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

    // --- 鬆らせ蠎ｧ讓吶・螳夂ｾｩ ---
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

    // --- 繧､繝ｳ繝・ャ繧ｯ繧ｹ縺ｮ螳夂ｾｩ・亥・蛛ｴ繧貞髄縺城・ｺ擾ｼ・---
    // 蜷・擇 [0,1,2][2,1,3] 縺ｮ繝代ち繝ｼ繝ｳ縺ｧ險・6蛟・
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


int32_t CreateJoint(const Node& node, const std::optional<int32_t>& parent, std::vector<Joint>& joints) {
    Joint joint;
    joint.name = node.name;
    joint.localMatrix = node.localMatrix;
    joint.skeletonSpaceMatrix = TransformFunctions::MakeIdentity4x4();
    joint.transform = node.transform;
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
    for (Joint& joint : skeleton.joints) {
        if (auto it = animation.nodeAnimations.find(joint.name); it != animation.nodeAnimations.end()) {
            const NodeAnimation& rootNodeAnimation = (*it).second;
            joint.transform.translate = CalculateValue(rootNodeAnimation.translate, animationTime);
            joint.transform.rotate = CalculateValue(rootNodeAnimation.rotate, animationTime);
            joint.transform.scale = CalculateValue(rootNodeAnimation.scale, animationTime);
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

