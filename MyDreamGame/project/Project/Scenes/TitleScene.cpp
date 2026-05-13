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
#include "Graphics/TextureManager.h"
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
    skyboxTextureHandle_ = TextureManager::GetInstance()->Load("Sprite/school/rostock_laage_airport_4k.dds", commandList_);

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
    
    // ■ RingEffectのルートオブジェクト作成
    ringEffectRoot_ = std::make_unique<PrimitiveObject>();
    ringEffectRoot_->Initialize(device.Get(), nullptr); // 描画しない
    ringEffectRoot_->SetName("RingEffect");
    ringEffectRoot_->SetTranslation({0.0f, 0.0f, 0.0f});

    // Ring 1
    {
        auto ring = std::make_unique<PrimitiveObject>();
        ring->Initialize(device.Get(), PrimitiveManager::GetInstance()->GetRing(0.5f, 1.0f, 64, 0.0f, 2.0f * 3.14159f, {1,1,1,1}, {1,1,1,1}, false));
        ring->GetMaterial().enableEnvironmentMap = 0;
        ring->GetMaterial().lightingType = 0;
        ring->SetTextureHandle(TextureManager::GetInstance()->GetGpuHandle(gradationHandle));
        ring->SetTranslation({0.0f, 0.0f, 0.0f});
        ring->SetRotation({0.4f, 0.0f, 0.0f}); // 少し傾ける
        ring->SetIsBillboard(false);          // 立体感を出すためにビルボードOFF
        ring->SetIsDoubleSided(true);
        ring->SetBlendMode(BlendMode::kBlendModeAdd);
        ring->SetName("Ring 1");
        ring->SetParent(ringEffectRoot_.get());
        primitiveParticles_.push_back(std::move(ring));
    }

    // Ring 2
    {
        auto ring = std::make_unique<PrimitiveObject>();
        ring->Initialize(device.Get(), PrimitiveManager::GetInstance()->GetRing(0.5f, 1.0f, 64, 0.0f, 2.0f * 3.14159f, {1,1,1,1}, {1,1,1,1}, false));
        ring->GetMaterial().enableEnvironmentMap = 0;
        ring->GetMaterial().lightingType = 0;
        ring->SetTextureHandle(TextureManager::GetInstance()->GetGpuHandle(gradationHandle));
        ring->SetTranslation({0.0f, 0.0f, 0.0f});
        ring->SetRotation({0.4f, 0.785f, 0.0f}); // 45度
        ring->SetIsBillboard(false);
        ring->SetIsDoubleSided(true);
        ring->SetBlendMode(BlendMode::kBlendModeAdd);
        ring->SetName("Ring 2");
        ring->SetParent(ringEffectRoot_.get());
        primitiveParticles_.push_back(std::move(ring));
    }

    // Ring 3
    {
        auto ring = std::make_unique<PrimitiveObject>();
        ring->Initialize(device.Get(), PrimitiveManager::GetInstance()->GetRing(0.5f, 1.0f, 64, 0.0f, 2.0f * 3.14159f, {1,1,1,1}, {1,1,1,1}, false));
        ring->GetMaterial().enableEnvironmentMap = 0;
        ring->GetMaterial().lightingType = 0;
        ring->SetTextureHandle(TextureManager::GetInstance()->GetGpuHandle(gradationHandle));
        ring->SetTranslation({0.0f, 0.0f, 0.0f});
        ring->SetRotation({0.4f, 1.57f, 0.0f});  // 90度
        ring->SetIsBillboard(false);
        ring->SetIsDoubleSided(true);
        ring->SetBlendMode(BlendMode::kBlendModeAdd);
        ring->SetName("Ring 3");
        ring->SetParent(ringEffectRoot_.get());
        primitiveParticles_.push_back(std::move(ring));
    }

    // Ring 4
    {
        auto ring = std::make_unique<PrimitiveObject>();
        ring->Initialize(device.Get(), PrimitiveManager::GetInstance()->GetRing(0.5f, 1.0f, 64, 0.0f, 2.0f * 3.14159f, {1, 1, 1, 1}, {1, 1, 1, 1}, false));
        ring->GetMaterial().enableEnvironmentMap = 0;
        ring->GetMaterial().lightingType = 0;
        ring->SetTextureHandle(TextureManager::GetInstance()->GetGpuHandle(gradationHandle));
        ring->SetTranslation({0.0f, 0.0f, 0.0f});
        ring->SetRotation({0.4f, 2.355f, 0.0f}); // 135度
        ring->SetIsBillboard(false);
        ring->SetIsDoubleSided(true);
        ring->SetBlendMode(BlendMode::kBlendModeAdd);
        ring->SetName("Ring 4");
        ring->SetParent(ringEffectRoot_.get());
        primitiveParticles_.push_back(std::move(ring));
    }
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

    // リングのアニメーション更新 (1秒周期でパッと出てゆっくり消える)
    ringEffectTimer_ += TimeManager::GetInstance().GetDeltaTime();
    if (ringEffectTimer_ > kRingEffectDuration) {
        ringEffectTimer_ = 0.0f; // ループ
    }

    // 1.0 -> 0.0 へフェードアウト
    float alpha = 1.0f - (ringEffectTimer_ / kRingEffectDuration);
    
    // リング全体の更新
    if (ringEffectRoot_) {
        ringEffectRoot_->Update();
    }
    for (auto& ring : primitiveParticles_) {
        ring->GetMaterial().color.w = alpha; // 透明度を適用
        ring->Update();
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

    // パーティクルの描画
#ifdef USE_IMGUI
    if (EditorManager::IsShowEffects()) {
#endif
        particleCommon_->DrawAll(viewProjectionMatrix);

        // ■ プリミティブパーティクルの描画
        for (auto& p : primitiveParticles_) {
            p->Draw(commandList_.Get());
        }
#ifdef USE_IMGUI
    }
#endif

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
    return result;
}

std::vector<PrimitiveObject *> TitleScene::GetPrimitives() {
    std::vector<PrimitiveObject *> result;
    if (ringEffectRoot_) {
        result.push_back(ringEffectRoot_.get());
    }
    // 子要素は返さないことでエディター上の表示を1つにまとめる
    return result;
}