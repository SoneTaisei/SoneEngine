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

	// 1. 画像をロードしてハンドルを取得 (TextureManagerに任せる)
	uint32_t uvCheckerIndex = TextureManager::GetInstance()->Load("resources/uvChecker.png", commandList_.Get());
	D3D12_GPU_DESCRIPTOR_HANDLE textureHandle = TextureManager::GetInstance()->GetGpuHandle(uvCheckerIndex);

	std::unique_ptr<Model> playerModel = std::make_unique<Model>();
	playerModel->Initialize(modelCommon_, "resources/plane", "plane.obj");
	playerModel->SetTextureHandle(textureHandle);
	playerModel->SetRotation({ 0.0f,0.0f,0.0f });
	playerModel_ = playerModel.get();
	models_.push_back(std::move(playerModel));

	// ==========================================================
	// ★ 2. スプライト（2D）の生成
	// ==========================================================
	// タイトルロゴのようなスプライトを作成します
	std::unique_ptr<Sprite> titleSprite = std::make_unique<Sprite>();

	// Initialize内で SpriteCommon に自動登録されます
	// ※ spriteCommon_ が正しく設定されている必要があります（IScene等で設定済みと仮定）
	titleSprite->Initialize(spriteCommon_, uvCheckerIndex);

	// 座標とサイズの設定 (画面左上が 0,0)
	titleSprite->SetPosition({ 300.0f, 100.0f }); // 画面中央あたり
	titleSprite->SetSize({ 640.0f, 360.0f });     // サイズ指定
	titleSprite->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f }); // 色（白＝そのまま）

	// リストに保存（これで所有権を管理）
	sprites_.push_back(std::move(titleSprite));
}

void TitleScene::Update(SceneManager *sceneManager) {
	// スペースキーが押されたらステージセレクトシーンへ
	if(KeyboardInput::GetInstance()->IsKeyPressed(DIK_SPACE)) {
		sceneManager->ChangeScene(new StageSelectScene());
	}
}

void TitleScene::Draw(const Matrix4x4 &viewProjectionMatrix) {
	/*textureHandle_ = TextureManager::GetInstance()->Load("resources/uvChecker.png",commandList_);
	D3D12_GPU_DESCRIPTOR_HANDLE planeGpuHandle = TextureManager::GetInstance()->GetGpuHandle(textureHandle_);*/

	// そのまま呼べる
	modelCommon_->DrawAll(viewProjectionMatrix);

	if(spriteCommon_) {
		spriteCommon_->PreDraw(commandList_.Get());
		spriteCommon_->DrawAll();
	}
}