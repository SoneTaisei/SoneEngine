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
#include <memory>
#include <optional>
#include <map>
#include <span>
#include <array>
#include "Quaternion.h"

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
#include "Vector2.h"
#include "Vector3.h"
#include "Vector4.h"
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
#include <vector>
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


struct AABB2D {
    float left;
    float top;
    float right;
    float bottom;
};

struct StageRoom {
    float x;
    float y;
    float width;
    float height;
};

struct  EulerTransform {
	Vector3 scale;
	Vector3 rotate;
	Vector3 translate;
};

struct QuaternionTransform {
    Vector3 scale;
    Quaternion rotate;
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
	Vector4 color;
};

struct Material {
    Vector4 color;
    int32_t lightingType;
    int32_t enableBlinnPhong;
    int32_t enableEnvironmentMap; // 環境マップ有効フラグ
    float alphaReference;         // アルファしきい値
    Matrix4x4 uvTransform;
    float shininess;
    float environmentCoefficient; // 環境マップ反射係数
    float dissolveThreshold;      // ディゾルブのしきい値
    float enableBoxMapping;       // ボックスマッピング有効フラグ
};

struct DirectionalLight {
	Vector4 color;//!< ライトの色
	Vector3 direction;//!< ライトの向き
	float intensity;//!< 輝度
	int32_t enableFlatShading; //!< フラットシェーディング
	float padding[3];
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
	float padding;
};

struct CameraForGPU {
    Vector3 worldPosition;
    float padding;
};

struct MaterialData {
	std::string textureFilePath;
};

struct Node {
    QuaternionTransform transform; // Transform情報
    Matrix4x4 localMatrix;      // ノードのローカル行列
    std::string name;           // ノード名
    std::vector<Node> children; // 子供のノード
};

struct Joint {
    QuaternionTransform transform; // Transform情報
    Matrix4x4 localMatrix; // localMatrix
    Matrix4x4 skeletonSpaceMatrix; // skeletonSpaceでの変換行列
    std::string name; // 名前
    std::vector<int32_t> children; // 子JointのIndexリスト。いなければ空
    int32_t index; // 自身のIndex
    std::optional<int32_t> parent; // 親JointのIndex。いなければnull
};

struct Skeleton {
    int32_t root; // RootJointのIndex
    std::map<std::string, int32_t> jointMap; // Joint名とIndexとの辞書
    std::vector<Joint> joints; // 所属しているジョイント
};

const uint32_t kNumMaxInfluence = 4;
// GPUに送る全頂点ごとのウェイトデータ
struct VertexInfluence {
    std::array<float, kNumMaxInfluence> weights;
    std::array<int32_t, kNumMaxInfluence> jointIndices;
};

// ロード時にBone側から見たウェイト情報
struct VertexWeightInfo {
    float weight;
    uint32_t vertexIndex;
};

struct JointWeightData {
    Matrix4x4 inverseBindPoseMatrix;
    std::vector<VertexWeightInfo> vertexWeights;
};

struct WellForGPU {
    Matrix4x4 skeletonSpaceMatrix;
    Matrix4x4 skeletonSpaceInverseTransposeMatrix;
};

struct SkinCluster {
    std::vector<Matrix4x4> inverseBindPoseMatrices;
    Microsoft::WRL::ComPtr<ID3D12Resource> influenceResource;
    D3D12_VERTEX_BUFFER_VIEW influenceBufferView;
    std::span<VertexInfluence> mappedInfluence;

    Microsoft::WRL::ComPtr<ID3D12Resource> paletteResource;
    std::span<WellForGPU> mappedPalette;
    std::pair<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE> paletteSrvHandle;
};

struct ModelData {
    std::vector<VertexData> vertices;
    std::vector<uint32_t> indices; // インデックス描画用
    MaterialData material;
    Node rootNode; // ルートノードを追加
    std::map<std::string, JointWeightData> skinClusterData; // スキニング用のウェイトと逆行列
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


// Skybox専用の頂点構造体
struct SkyboxVertexData {
    Vector4 position;
};
