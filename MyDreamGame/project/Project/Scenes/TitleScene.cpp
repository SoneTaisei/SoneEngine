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
#include "Graphics/CameraManager.h"
#include "Renderer/DirectXCommon/DirectXCommon.h"

TitleScene::~TitleScene() {
}

void TitleScene::Initialize(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList) {
    commandList_ = commandList;
    Microsoft::WRL::ComPtr<ID3D12Device> device;
    commandList->GetDevice(IID_PPV_ARGS(&device));

    cameraTransform_.translate = {0.0f, 0.0f, -10.0f};

    // 1. マネージャからモデル（素材）を取得（なければロードされる）
    Model *planeModel = ModelManager::GetInstance()->GetModel("Object/School/plane", "plane.gltf");

    // 2. Object3D（実体）を生成して初期化
    auto planeObject = std::make_unique<Object3D>();
    planeObject->Initialize(device.Get(), planeModel);

    // 3. 座標やテクスチャの設定（Object3Dに対して行う！）
    uint32_t planeIndex = TextureManager::GetInstance()->Load("Sprite/School/uvChecker.png", commandList_);
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
    //sprites_.push_back(std::move(sprite));

    // ★ Skyboxの初期化処理を追加
    // 1. テクスチャをロード
    skyboxTextureHandle_ = TextureManager::GetInstance()->Load("Sprite/Original/skybox/skybox_highres_build.dds", commandList_);
    //skyboxTextureHandle_ = TextureManager::GetInstance()->Load("Sprite/school/rostock_laage_airport_4k.dds", commandList_);

    // 2. インスタンスを生成
    skybox_ = std::make_unique<Skybox>();

    // 3. 初期化（※dxCommon_ の取得方法はエンジンの設計に合わせてください！）
    // もし TitleScene に dxCommon_ が無い場合は、DirectXCommon::GetInstance() などを使うか、
    // SceneManager から引っ張ってくる必要があります。
    skybox_->Initialize(device.Get(), skyboxTextureHandle_);
    Object3D::SetEnvironmentMapHandle(TextureManager::GetInstance()->GetGpuHandle(skyboxTextureHandle_));

    debugCamera_ = std::make_unique<DebugCamera>();
    debugCamera_->Initialize(1280, 720);

    // ■ プリミティブパーティクルの初期化
    PrimitiveManager::GetInstance()->Initialize(device.Get());
    
    // 8個のパーティクルを生成
    for (int i = 0; i < 8; ++i) {
        auto pObj = std::make_unique<PrimitiveObject>();
        // ランダムな形状を選択
        PrimitiveType type = static_cast<PrimitiveType>(rand() % 9);
        pObj->Initialize(device.Get(), PrimitiveManager::GetInstance()->GetPrimitive(type));
        
        pObj->SetScale({0.05f, 2.5f, 0.05f});
        pObj->SetTextureHandle(TextureManager::GetInstance()->GetGpuHandle(planeIndex));
        pObj->GetMaterial().color = {1.0f, 1.0f, 1.0f, 0.0f}; // 最初は透明
        
        primitiveParticles_.push_back(std::move(pObj));
    }
}

void TitleScene::Update(SceneManager *sceneManager) {
    // シーン遷移処理
    if (KeyboardInput::GetInstance()->IsKeyPressed(DIK_SPACE)) {
        sceneManager->ChangeScene(std::make_unique<StageSelectScene>());
    }

    if (debugCamera_) {
        debugCamera_->Update();
        // ★ debugCamera_->Update() の中で CameraManager::GetInstance()->SetCameraInfo(...) 
        //    が自動的に呼ばれるため、ここでの手動セットは不要です。
    }

    // 全オブジェクトの更新（座標変換行列の計算など）
    for (auto &object : objects_) {
        object->Update();
    }

#ifdef _DEBUG
    // ImGuiもObject3D版を呼ぶ
    ImGui::Begin("Objects");
    for (auto &object : objects_) {
        ShowObject3DGui("Object", object.get());
    }
    ImGui::End();
#endif

    for (auto &sprite : sprites_) {
        sprite->Update();
    }

    if (skybox_) {
        skybox_->Update();
    }

    // ■ プリミティブパーティクルの更新 (フラッシュ/フェード演出)
    static float flashTimer = 0.0f;
    flashTimer += 1.0f / 60.0f;
    bool shouldReset = (flashTimer > 0.6f); // 0.6秒ごとにパッと表示
    if (shouldReset) flashTimer = 0.0f;

    for (size_t i = 0; i < primitiveParticles_.size(); ++i) {
        auto& p = primitiveParticles_[i];
        
        if (shouldReset) {
            // リセット：ランダムな角度でパッと表示
            float angle = (float(rand()) / RAND_MAX) * 2.0f * 3.141592f;
            float dist = 0.3f; // 中心からの距離
            p->SetTranslation({cosf(angle) * dist, sinf(angle) * dist, 0.0f});
            p->SetScale({0.05f, 2.5f, 0.05f});
            p->SetRotation({0.0f, 0.0f, angle - 1.5708f}); // 角度に合わせて回転
            p->GetMaterial().color.w = 1.0f;
        } else {
            p->GetMaterial().color.w *= 0.85f;
        }

        p->Update();
    }
}

void TitleScene::Draw(const Matrix4x4 &viewProjectionMatrix) {
    // ★ モデル描画の前準備
    if (modelCommon_) {
        modelCommon_->PreDraw(commandList_.Get());
    }

    // 各オブジェクトに「自分の行列で描画して！」と頼む
    for (auto &object : objects_) {
        object->Draw(commandList_.Get());
    }

    // ★ 3Dオブジェクトの直後にSkyboxを描画！
    if (skybox_) {
        skybox_->Draw(commandList_.Get());
        
        // ★ Skyboxの描画後はPSOが切り替わってしまうため、再度モデル用の設定を呼び出す
        auto dxCommon = DirectXCommon::GetInstance();
        commandList_.Get()->SetGraphicsRootSignature(dxCommon->GetRootSignature());
        commandList_.Get()->SetPipelineState(dxCommon->GetGraphicsPipelineState());

        if (modelCommon_) {
            modelCommon_->PreDraw(commandList_.Get());
        }
    }

    // ■ プリミティブパーティクルの描画
    for (auto& p : primitiveParticles_) {
        p->Draw(commandList_.Get());
    }

    // -------------------------------------------------
    // ■ パーティクルの描画
    // -------------------------------------------------
    if (particleCommon_) {
        // 前処理
        particleCommon_->PreDraw(commandList_.Get());

        // 一括描画 (引数の viewProjectionMatrix を渡す)
        particleCommon_->DrawAll(viewProjectionMatrix);
    }

    if (spriteCommon_) {
        spriteCommon_->PreDraw(commandList_.Get());
        spriteCommon_->DrawAll();
    }
}