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
#include "Scene/SceneFactory.h"
#include "Renderer/DirectXCommon/DirectXCommon.h"

TitleScene::~TitleScene() {
}

void TitleScene::OnEnter(SceneManager* sceneManager) {
    // シーン遷移時の開始処理
}

void TitleScene::OnExit(SceneManager* sceneManager) {
    // シーン遷移時の終了処理
}

void TitleScene::Initialize() {
    
    Microsoft::WRL::ComPtr<ID3D12Device> device;
    device = DirectXCommon::GetInstance()->GetDevice();

    cameraTransform_.translate = {0.0f, 0.0f, -10.0f};

    // --- 各種共通管理クラスの初期化 ---
    modelCommon_ = std::make_unique<ModelCommon>();
    modelCommon_->Initialize(device.Get());

    spriteCommon_ = std::make_unique<SpriteCommon>();
    spriteCommon_->Initialize(DirectXCommon::GetInstance(), 1280, 720);

    particleCommon_ = std::make_unique<ParticleCommon>();
    particleCommon_->Initialize(device.Get());

    // 1. マネージャからモデル（素材）を取得（なければロードされる）
    Model *planeModel = ModelManager::GetInstance()->GetModel("resources/Object/School/plane", "plane.gltf");

    // 2. Object3D（実体）を生成して初期化
    auto planeObject = std::make_unique<Object3D>();
    planeObject->Initialize(device.Get(), planeModel);

    // 3. 座標やテクスチャの設定（Object3Dに対して行う！）
    uint32_t planeIndex = TextureManager::GetInstance()->Load("resources/Sprite/School/uvChecker.png");
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
    skyboxTextureHandle_ = TextureManager::GetInstance()->Load("resources/Sprite/Original/skybox/skybox_highres_build.dds");
    //skyboxTextureHandle_ = TextureManager::GetInstance()->Load("resources/Sprite/Original/yakei/skybox.dds");
    //skyboxTextureHandle_ = TextureManager::GetInstance()->Load("resources/Sprite/Original/yakei/panoramic-view-beach-sunset.dds");
    //skyboxTextureHandle_ = TextureManager::GetInstance()->Load("resources/Sprite/School/rostock_laage_airport_4k.dds");
    //skyboxTextureHandle_ = TextureManager::GetInstance()->Load("resources/Sprite/Original/bat_miyazaki/IMG_2496.dds");
    //skyboxTextureHandle_ = TextureManager::GetInstance()->Load("resources/Sprite/Original/bat_miyazaki/IMG_2496_direct.dds");
    //skyboxTextureHandle_ = TextureManager::GetInstance()->Load("resources/Sprite/Original/bat_miyazaki/IMG_2496_dxt5.dds");

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
    uint32_t gradationHandle = TextureManager::GetInstance()->Load("resources/Sprite/School/gradationLine.png");
    
    // ■ RingEffectの作成
    ringEffect_ = std::make_unique<RingEffect>();
    ringEffect_->Initialize(device.Get(), gradationHandle);

    // ■ CylinderEffectの作成
    cylinderEffect_ = std::make_unique<CylinderEffect>();
    cylinderEffect_->Initialize(device.Get(), gradationHandle);
}

void TitleScene::Update(SceneManager *sceneManager) {
    // シーン遷移直後の同一フレームでのSPACEキー入力を拾わないようにする
    if (isFirstFrame_) {
        isFirstFrame_ = false;
    } else {
        if (KeyboardInput::GetInstance()->IsKeyPressed(DIK_SPACE)) {
            sceneManager->ChangeScene(SceneFactory::CreateScene(SceneType::kStageSelect));
            return;
        }
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
}

void TitleScene::Draw(const Matrix4x4 &viewProjectionMatrix) {
    // ★ モデル描画の前準備
    if (modelCommon_) {
        modelCommon_->PreDraw();
    }

    // ★ 3Dオブジェクトの直後にSkyboxを描画！
    if (skybox_) {
        skybox_->Draw();
        
        // ★ Skyboxの描画後はPSOが切り替わってしまうため、再度モデル用の設定を呼び出す
        auto dxCommon = DirectXCommon::GetInstance();
        DirectXCommon::GetInstance()->GetCommandList()->SetGraphicsRootSignature(dxCommon->GetRootSignature());
        DirectXCommon::GetInstance()->GetCommandList()->SetPipelineState(dxCommon->GetGraphicsPipelineState());

        if (modelCommon_) {
            modelCommon_->PreDraw();
        }
    }

    // 3Dモデル的描画
#ifdef USE_IMGUI
    if (EditorManager::IsShowObjects()) {
#endif
        for (auto &object : objects_) {
            object->Draw();
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
            particleCommon_->PreDraw();
            particleCommon_->DrawAll(viewProjectionMatrix);
        }

        // ■ プリミティブパーティクルの描画
        if (ringEffect_) {
            ringEffect_->Draw();
        }
        if (cylinderEffect_) {
            cylinderEffect_->Draw();
        }
#ifdef USE_IMGUI
    }
#endif

    if (spriteCommon_) {
        spriteCommon_->PreDraw();
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
}

void TitleScene::DisplayImGui(PrimitiveObject* selectedPrimitive) {
#ifdef USE_IMGUI
    // エディター側でプレイ状態になっていないときは、タイトルUIを描画しない
    if (!EditorManager::IsPlaying()) {
        return;
    }

    ImGui::SetNextWindowPos(ImVec2(1280.0f / 2.0f, 720.0f / 2.0f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::Begin("TitleUI", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_AlwaysAutoResize);
    
    ImGui::SetWindowFontScale(4.0f);
    float windowWidth = ImGui::GetWindowSize().x;
    const char* titleText = "My Dream Game";
    float textWidth = ImGui::CalcTextSize(titleText).x;
    ImGui::SetCursorPosX((windowWidth - textWidth) * 0.5f);
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "%s", titleText);
    
    ImGui::SetWindowFontScale(2.0f);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 50.0f);
    const char* startText = "Press SPACE to Start";
    float startTextWidth = ImGui::CalcTextSize(startText).x;
    ImGui::SetCursorPosX((windowWidth - startTextWidth) * 0.5f);
    
    static float time = 0.0f;
    time += ImGui::GetIO().DeltaTime;
    float alpha = (sinf(time * 5.0f) + 1.0f) * 0.5f;
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, alpha), "%s", startText);
    
    ImGui::End();
#endif
}