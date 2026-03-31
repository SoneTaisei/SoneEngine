#include "TitleScene.h"
#include "../externals/imgui/imgui.h"
#include "Core/TimeManager.h"
#include "Graphics/TextureManager.h"
#include "Input/KeyboardInput.h"
#include "Resource/Model/ModelCommon.h"
#include "Scene/SceneManager.h"
#include "Resource/Sprite/SpriteCommon.h"
#include "StageSelectScene.h"
#include <wrl.h>
#include "Core/Utility/ImGuiHelper.h"
#include "Resource/Model/ModelManager.h"

TitleScene::~TitleScene() {
}

void TitleScene::Initialize(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList) {
    commandList_ = commandList;
    Microsoft::WRL::ComPtr<ID3D12Device> device;
    commandList->GetDevice(IID_PPV_ARGS(&device));

    cameraTransform_.translate = {0.0f, 0.0f, -10.0f};

    // 1. マネージャからモデル（素材）を取得（なければロードされる）
    Model *planeModel = ModelManager::GetInstance()->GetModel("Object/plane", "plane.gltf");

    // 2. Object3D（実体）を生成して初期化
    auto planeObject = std::make_unique<Object3D>();
    planeObject->Initialize(device.Get(), planeModel);

    // 3. 座標やテクスチャの設定（Object3Dに対して行う！）
    uint32_t planeIndex = TextureManager::GetInstance()->Load("Sprite/uvChecker.png", commandList_);
    planeObject->SetTextureHandle(TextureManager::GetInstance()->GetGpuHandle(planeIndex));
    planeObject->SetRotation({0.0f, 0.0f, 0.0f});

    objects_.push_back(std::move(planeObject));

    // ② Spriteのインスタンスを生成
    auto sprite = std::make_unique<Sprite>();

    // ③ 初期化 (spriteCommon_はIScene等で定義されている前提)
    sprite->Initialize(spriteCommon_, planeIndex);

    // ④ 位置やサイズなどのパラメータを設定
    // 画面中央付近に配置する例
    sprite->SetPosition({640.0f, 360.0f}); // 画面中央付近など
    sprite->SetSize({200.0f, 200.0f});     // しっかり見える大きさにする

    // ⑤ 管理用の配列に追加して保持する
    sprites_.push_back(std::move(sprite));
}

void TitleScene::Update(SceneManager *sceneManager) {
    // シーン遷移処理
    if (KeyboardInput::GetInstance()->IsKeyPressed(DIK_SPACE)) {
        sceneManager->ChangeScene(std::make_unique<StageSelectScene>());
    }

    // 1. カメラのTransformからビュー行列を作成
    // cameraTransform_ は TitleScene.h で宣言されているものを使います
    Matrix4x4 viewMatrix = TransformFunctions::MakeViewMatrix(cameraTransform_.rotate, cameraTransform_.translate);

    // 2. プロジェクション行列（透視投影行列）を作成
    // 一般的な設定：視野角0.45rad, アスペクト比16:9, 近平面0.1, 遠平面1000.0
    Matrix4x4 projectionMatrix = TransformFunctions::MakePerspectiveFovMatrix(0.45f, 1280.0f / 720.0f, 0.1f, 1000.0f);

// 全オブジェクトの更新（座標変換行列の計算など）
    for (auto &object : objects_) {
        object->Update(viewMatrix, projectionMatrix);
    }

    // ImGuiもObject3D版を呼ぶ
    for (auto &object : objects_) {
        ShowObject3DGui("Object", object.get());
    }

    for (auto &sprite : sprites_) {
        sprite->Update();
    }

   
}

void TitleScene::Draw(const Matrix4x4 &viewProjectionMatrix) {
    // 既存の描画
    // 各オブジェクトに「自分の行列で描画して！」と頼む
    for (auto &object : objects_) {
        object->Draw(commandList_.Get());
    }

    // -------------------------------------------------
    // ■ パーティクルの描画
    // -------------------------------------------------
    if (particleCommon_) {
        // 前処理
        particleCommon_->PreDraw(commandList_.Get());

        // 一括描画 (viewProjectionMatrixを渡す)
        particleCommon_->DrawAll(viewProjection_);
    }

    if (spriteCommon_) {
        spriteCommon_->PreDraw(commandList_.Get());
        spriteCommon_->DrawAll();
    }
}