#include "TitleScene.h"
#include "SceneManager.h"
#include "Input/KeyboardInput.h"
#include "../externals/imgui/imgui.h"
#include "Sprite/SpriteCommon.h"
#include "Model/ModelCommon.h"
#include "Graphics/TextureManager.h"
#include "Core/TimeManager.h"
#include "StageSelectScene.h"
#include <wrl.h>

TitleScene::~TitleScene() {
}

void TitleScene::Initialize(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList) {
	commandList_ = commandList;

	// 1. まず共通部分 (ParticleCommon) を生成して初期化
	// これをしないと次の行でクラッシュします
	particleCommon_ = new ParticleCommon();
	particleCommon_->Initialize(device.Get()); // ★ここで本物の device が必要

	// 2. メンバ変数 particle_ を生成（ローカル変数にしない！）
	particle_ = new Particle();

	// 3. 初期化
	particle_->Initialize(commandList.Get(), particleCommon_, 10, "resources/uvChecker.png", 10);
}

void TitleScene::Update(SceneManager *sceneManager) {
	// スペースキーが押されたらステージセレクトシーンへ
	if(KeyboardInput::GetInstance()->IsKeyPressed(DIK_SPACE)) {
		sceneManager->ChangeScene(new StageSelectScene());
	}
}

void TitleScene::Draw(const Matrix4x4 &viewProjectionMatrix) {
	textureHandle_ = TextureManager::GetInstance()->Load("resources/uvChecker.png",commandList_);
	D3D12_GPU_DESCRIPTOR_HANDLE planeGpuHandle = TextureManager::GetInstance()->GetGpuHandle(textureHandle_);

	// そのまま呼べる
	modelCommon_->DrawAll(viewProjectionMatrix);

	particleCommon_->DrawAll();

	if(spriteCommon_) {
		spriteCommon_->PreDraw(commandList_.Get());
		spriteCommon_->DrawAll();
	}
}