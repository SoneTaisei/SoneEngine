#pragma once

#include <Windows.h>

#define _USE_MATH_DEFINES // M_PIなどの定数を使うために必要
#include <cmath>          // sin, cos, M_PI など数学関数

#include<format>
#include <string>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <chrono>

#include <d3d12.h>
#pragma comment(lib,"d3d12.lib")
#include <Dxgi1_6.h>

#pragma comment(lib,"dxgi.lib")
#include <cassert>

// debug用の処理を作るときに使う呼び出し
#include <dbghelp.h>
#pragma comment(lib,"Dbghelp.lib")

// StringCcPrintfWを使うときに作る呼び出し
#include<strsafe.h>

// 最後の警告を作るときに使う呼び出し
#include <dxgidebug.h>
#pragma comment(lib,"dxguid.lib")
#include<dxcapi.h>
#pragma comment(lib,"dxcompiler.lib")

// Transformするための呼び出し
#include "TransformFunctions.h"

// ImGuiを使うための宣言
#include "../externals/imgui/imgui.h"
#include "imgui_impl_dx12.h"
#include "imgui_impl_win32.h"
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// DirectXを使うための宣言
#include "../externals/DirectXTex/DirectXTex.h"  // パスはプロジェクト構成により調整
#pragma comment(lib, "windowscodecs.lib")

#include"../externals/DirectXTex/d3dx12.h"
#include"vector"

#include <sstream>

#include <wrl.h>

#include<xaudio2.h>

#pragma comment(lib,"xaudio2.lib")

#define DIRECTINPUT_VERSION 0x0800
#include<dinput.h>

#pragma comment(lib,"dinput8.lib")

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

/*********************************************************
*構造体
*********************************************************/

struct Vector2 {
	float x;
	float y;
};

struct Vector4 {
	float x;
	float y;
	float z;
	float w;
};

struct  Transform {
	Vector3 scale;
	Vector3 rotate;
	Vector3 translate;
};

struct TransformMatrix {
	Matrix4x4 WVP;
	Matrix4x4 World;
    Matrix4x4 WorldInverseTranspose;
};

struct VertexData {
	Vector4 position;
	Vector2 texcoord;
	Vector3 normal;
};

struct Material {
	Vector4 color;
	int32_t lightingType;
    int32_t enableBlinnPhong;
    float padding[2];
	Matrix4x4 uvTransform;
    float shininess;
};

struct DirectionalLight {
	Vector4 color;//!< ライトの色
	Vector3 direction;//!< ライトの向き
	float intensity;//!< 輝度
};

struct PointLight {
    Vector4 color;    //!< ライトの色
    Vector3 position; //!< ライトの位置
    float intensity;  //!< 輝度
    float radius;     //!< ライトの届く最大距離
    float decay;      //!< 減衰率
    float padding[2]; //!< 16バイト境界に合わせるためのパディング
};

struct SpotLight {
    Vector4 color;
    Vector3 position;
    float intensity;
    Vector3 direction;
    float distance;
	float decay;
    float cosAngle;
    float cosFalloffStart;
    float padding[2];
};

struct ViewProjectionData {
	Matrix4x4 viewProjectionMatrix;
	Vector3 cameraPosition;
	float padding;  // ← これを忘れず追加！
};

struct MaterialData {
	std::string textureFilePath;
};

struct Node {
    Matrix4x4 localMatrix;      // ノードのローカル行列
    std::string name;           // ノード名
    std::vector<Node> children; // 子供のノード
};

struct ModelData {
    std::vector<VertexData> vertices;
    std::vector<uint32_t> indices; // インデックス描画用
    MaterialData material;
    Node rootNode; // ルートノードを追加
};

struct ChunkHeader {
	char id[4];// チャンク毎のID
	int32_t size;// チャンクサイズ
};

struct RiffHeader {
	ChunkHeader chunk;// "RIFF"
	char type[4];// "WAVE"
};

struct FormatChunk {
	ChunkHeader chunk;// "fmt"
	WAVEFORMATEX fmt;// 波形フォーマット
};

struct SoundData {
	// 波形フォーマット
	WAVEFORMATEX wfex;
	// バッファの先頭アドレス
    std::unique_ptr<BYTE[]> pBuffer;
	// バッファのサイズ
	unsigned int bufferSize;
};

inline Vector3 operator*(const Matrix4x4 &mat, const Vector3 &vec) {
	Vector3 result;
	// 方向ベクトルのため、w成分は0として計算
	result.x = vec.x * mat.m[0][0] + vec.y * mat.m[1][0] + vec.z * mat.m[2][0];
	result.y = vec.x * mat.m[0][1] + vec.y * mat.m[1][1] + vec.z * mat.m[2][1];
	result.z = vec.x * mat.m[0][2] + vec.y * mat.m[1][2] + vec.z * mat.m[2][2];
	// w成分は方向ベクトルのため無視
	return result;
}

inline Vector3 operator*(float scalar, const Vector3 &vec) {
	return vec * scalar;
}

inline Matrix4x4 operator*(const Matrix4x4 &m1, const Matrix4x4 &m2) {
	Matrix4x4 result{};

	for(int row = 0; row < 4; ++row) {
		for(int col = 0; col < 4; ++col) {
			result.m[row][col] =
				m1.m[row][0] * m2.m[0][col] +
				m1.m[row][1] * m2.m[1][col] +
				m1.m[row][2] * m2.m[2][col] +
				m1.m[row][3] * m2.m[3][col];
		}
	}

	return result;
}


