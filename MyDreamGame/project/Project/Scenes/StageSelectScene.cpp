#include "StageSelectScene.h"
#include "../externals/imgui/imgui.h"
#include "Graphics/TextureManager.h"
#include "Input/KeyboardInput.h"
#include "Scene/SceneManager.h"
#include "Core/Utility/ImGuiHelper.h"
#include "Scenes/GameScene.h"
#include "Resource/Model/ModelManager.h"

StageSelectScene::~StageSelectScene() {
}

void StageSelectScene::Initialize(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList) {

    commandList_ = commandList;

    // Deviceの取得
    Microsoft::WRL::ComPtr<ID3D12Device> device;
    commandList->GetDevice(IID_PPV_ARGS(&device));

    // 1. マネージャから素材を借りる（頂点バッファを重複させない！）
    Model *skydomeModelResource = ModelManager::GetInstance()->GetModel("Object/Original/sphere", "sphere.gltf");
    uint32_t skydomeIndex = TextureManager::GetInstance()->Load("Sprite/School/monsterBall.png", commandList_);
    D3D12_GPU_DESCRIPTOR_HANDLE skydomeTH = TextureManager::GetInstance()->GetGpuHandle(skydomeIndex);

    // 2. 実体(Object3D)を作る
    auto skydomeObject = std::make_unique<Object3D>();
    skydomeObject->Initialize(device.Get(), skydomeModelResource);

    // 3. 個別の設定をする
    skydomeObject->SetTextureHandle(skydomeTH);
    skydomeObject->SetRotation({0.0f, 0.0f, 0.0f});

    cameraTransform_.translate = {0.0f, 0.0f, -10.0f};

    objects_.push_back(std::move(skydomeObject));
}

void StageSelectScene::Update(SceneManager *sceneManager) {
    // 1. カメラのTransformからビュー行列を作成
    // cameraTransform_ は TitleScene.h で宣言されているものを使います
    Matrix4x4 viewMatrix = TransformFunctions::MakeViewMatrix(cameraTransform_.rotate, cameraTransform_.translate);

    // 2. プロジェクション行列（透視投影行列）を作成
    // 一般的な設定：視野角0.45rad, アスペクト比16:9, 近平面0.1, 遠平面1000.0
    Matrix4x4 projectionMatrix = TransformFunctions::MakePerspectiveFovMatrix(0.45f, 1280.0f / 720.0f, 0.1f, 1000.0f);

    // 全オブジェクトの更新（座標変換行列の計算など）
    for (auto &object : objects_) {
        object->Update();
    }

    // ImGuiもObject3D版を呼ぶ
    for (auto &object : objects_) {
        ShowObject3DGui("Object", object.get());
    }

    // シーン遷移の処理
    if (KeyboardInput::GetInstance()->IsKeyPressed(DIK_SPACE)) {
        sceneManager->ChangeScene(std::make_unique<GameScene>());
    }
}

void StageSelectScene::Draw(const Matrix4x4 &viewProjectionMatrix) {
    // 各オブジェクトに「自分の行列で描画して！」と頼む
    for (auto &object : objects_) {
        object->Draw(commandList_.Get());
    }
}
