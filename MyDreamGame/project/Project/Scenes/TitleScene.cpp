#include "TitleScene.h"
#include "../externals/imgui/imgui.h"
#include "Core/TimeManager.h"
#include "Graphics/TextureManager.h"
#include "Input/KeyboardInput.h"
#include "Resource/Model/ModelCommon.h"
#include "Scene/SceneManager.h"
#include "Resource/Sprite/SpriteCommon.h"
#include <wrl.h>
#include "Resource/Model/ModelManager.h"
#include "Graphics/CameraManager.h"
#ifdef USE_IMGUI
#include "Editor/EditorManager.h"
#endif
#include "Renderer/DirectXCommon/DirectXCommon.h"

TitleScene::~TitleScene() {
}

void TitleScene::Initialize(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList) {
    commandList_ = commandList;
    Microsoft::WRL::ComPtr<ID3D12Device> device;
    commandList->GetDevice(IID_PPV_ARGS(&device));

    cameraTransform_.translate = {0.0f, 0.0f, -10.0f};

    // --- 各種共通管理クラスの初期化 ---
    modelCommon_ = std::make_unique<ModelCommon>();
    modelCommon_->Initialize(device.Get());

    spriteCommon_ = std::make_unique<SpriteCommon>();
    spriteCommon_->Initialize(DirectXCommon::GetInstance(), 1280, 720);

    particleCommon_ = std::make_unique<ParticleCommon>();
    particleCommon_->Initialize(device.Get());

    // 1. マネージャからモデル（素材）を取得（なければロードされる）
    Model *planeModel = ModelManager::GetInstance()->GetModel("Object/School/plane", "plane.gltf");

    // 2. Object3D（実体）を生成して初期化
    auto planeObject = std::make_unique<Object3D>();
    planeObject->Initialize(device.Get(), planeModel);

    // 3. 座標やテクスチャの設定（Object3Dに対して行う！）
    uint32_t planeIndex = TextureManager::GetInstance()->Load("Sprite/School/uvChecker.png", commandList_);
    planeObject->SetTextureHandle(TextureManager::GetInstance()->GetGpuHandle(planeIndex));
    planeObject->SetRotation({0.0f, 0.0f, 0.0f});
    planeObject->SetName("Ground Plane");

    objects_.push_back(std::move(planeObject));

    // ② Spriteのインスタンスを生成
    auto sprite = std::make_unique<Sprite>();

    // ③ 初期化 (spriteCommon_はIScene等で定義されている前提)
    sprite->Initialize(spriteCommon_.get(), planeIndex);

    // ④ 位置やサイズなどのパラメータを設定
    // 画面中央付近に配置する例
    sprite->SetPosition({640.0f, 360.0f}); // 画面中央付近など
    sprite->SetSize({200.0f, 200.0f});     // しっかり見える大きさにする

    // ⑤ 管理用の配列に追加して保持する
    //sprites_.push_back(std::move(sprite));

    // ★ Skyboxの初期化処理を追加
    // 1. テクスチャをロード
    //skyboxTextureHandle_ = TextureManager::GetInstance()->Load("Sprite/Original/skybox/skybox_highres_build.dds", commandList_);
    skyboxTextureHandle_ = TextureManager::GetInstance()->Load("Sprite/Original/yakei/skybox.dds", commandList_);
    //skyboxTextureHandle_ = TextureManager::GetInstance()->Load("Sprite/School/rostock_laage_airport_4k.dds", commandList_);
    skyboxTextureHandle_ = TextureManager::GetInstance()->Load("Sprite/Original/bat_miyazaki/IMG_2496.dds", commandList_);
    skyboxTextureHandle_ = TextureManager::GetInstance()->Load("Sprite/Original/bat_miyazaki/IMG_2496_direct.dds", commandList_);

    // 2. インスタンスを生成
    skybox_ = std::make_unique<Skybox>();

    // 3. 初期化（※dxCommon_ の取得方法はエンジンの設計に合わせてください！）
    // もし TitleScene に dxCommon_ が無い場合は、DirectXCommon::GetInstance() などを使うか、
    // SceneManager から引っ張ってくる必要があります。
    skybox_->Initialize(device.Get(), skyboxTextureHandle_);
    Object3D::SetEnvironmentMapHandle(TextureManager::GetInstance()->GetGpuHandle(skyboxTextureHandle_));

    debugCamera_ = std::make_unique<DebugCamera>();
    debugCamera_->Initialize(1280, 720);

    // ■ 拡張Ringプリミティブのデモ実装
    PrimitiveManager::GetInstance()->Initialize(device.Get());
    uint32_t gradationHandle = TextureManager::GetInstance()->Load("Sprite/School/gradationLine.png", commandList_);
    
    // ■ RingEffectの作成
    ringEffect_ = std::make_unique<RingEffect>();
    ringEffect_->Initialize(device.Get(), gradationHandle);

    // ■ CylinderEffectの作成
    cylinderEffect_ = std::make_unique<CylinderEffect>();
    cylinderEffect_->Initialize(device.Get(), gradationHandle);

    // ■ HitEffectの作成
    hitEffect_ = std::make_unique<HitEffect>();
    hitEffect_->Initialize(commandList_.Get(), particleCommon_.get(), 1024, "Sprite/School/circle2.png", 112, kBlendModeAdd);
}

void TitleScene::Update(SceneManager *sceneManager) {
    if (debugCamera_) {
        debugCamera_->Update();
        // ★ debugCamera_->Update() の中で CameraManager::GetInstance()->SetCameraInfo(...) 
        //    が自動的に呼ばれるため、ここでの手動セットは不要です。
    }

    // 全オブジェクトの更新（座標変換行列の計算など）
    for (auto &object : objects_) {
        object->Update();
    }


    for (auto &sprite : sprites_) {
        sprite->Update();
    }

    if (skybox_) {
        skybox_->Update();
    }

    // エフェクトの更新
    float deltaTime = TimeManager::GetInstance().GetDeltaTime();
    if (ringEffect_) {
        ringEffect_->Update(deltaTime);
    }
    if (cylinderEffect_) {
        cylinderEffect_->Update(deltaTime);
    }
    if (hitEffect_) {
        hitEffect_->Update();
    }
}

void TitleScene::Draw(const Matrix4x4 &viewProjectionMatrix) {
    // ★ モデル描画の前準備
    if (modelCommon_) {
        modelCommon_->PreDraw(commandList_.Get());
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

    // 3Dモデルの描画
#ifdef USE_IMGUI
    if (EditorManager::IsShowObjects()) {
#endif
        for (auto &object : objects_) {
            object->Draw(commandList_.Get());
        }
#ifdef USE_IMGUI
    }
#endif

    // -------------------------------------------------
    // ■ エフェクト/パーティクルの描画
    // -------------------------------------------------
#ifdef USE_IMGUI
    if (EditorManager::IsShowEffects()) {
#endif
        if (particleCommon_) {
            particleCommon_->PreDraw(commandList_.Get());
            particleCommon_->DrawAll(viewProjectionMatrix);
        }
        if (hitEffect_) {
            hitEffect_->Draw(viewProjectionMatrix);
        }

        // ■ プリミティブパーティクルの描画
        if (ringEffect_) {
            ringEffect_->Draw(commandList_.Get());
        }
        if (cylinderEffect_) {
            cylinderEffect_->Draw(commandList_.Get());
        }
#ifdef USE_IMGUI
    }
#endif

    if (spriteCommon_) {
        spriteCommon_->PreDraw(commandList_.Get());
        spriteCommon_->DrawAll();
    }
}

std::vector<Object3D *> TitleScene::GetObjects() {
    std::vector<Object3D *> result;
    for (auto &obj : objects_) {
        result.push_back(obj.get());
    }
    return result;
}

std::vector<ParticleManager *> TitleScene::GetParticles() {
    std::vector<ParticleManager *> result;
    for (auto &p : particles_) {
        result.push_back(p.get());
    }
    if (hitEffect_) {
        result.push_back(hitEffect_.get());
    }
    return result;
}

std::vector<PrimitiveObject *> TitleScene::GetPrimitives() {
    std::vector<PrimitiveObject *> result;
    if (ringEffect_) {
        result.push_back(ringEffect_->GetRoot());
    }
    if (cylinderEffect_) {
        result.push_back(cylinderEffect_->GetRoot());
    }
    // 子要素は返さないことでエディター上の表示を1つにまとめる
    return result;
}

void TitleScene::UpdateEditor() {
    for (auto &object : objects_) {
        object->Update();
    }
    for (auto &sprite : sprites_) {
        sprite->Update();
    }
    if (skybox_) {
        skybox_->Update();
    }
    if (ringEffect_) {
        ringEffect_->Update(0.0f);
    }
    if (cylinderEffect_) {
        cylinderEffect_->Update(0.0f);
    }
    if (hitEffect_) {
        hitEffect_->Update();
    }
}