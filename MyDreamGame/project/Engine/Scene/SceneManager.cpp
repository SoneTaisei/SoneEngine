#include "SceneManager.h"
#include <cassert>
#include "Resource/Sprite/SpriteCommon.h"
#include "Resource/Model/ModelCommon.h"
#include "Effect/ParticleCommon.h"
#ifdef USE_IMGUI
#include "Editor/EditorManager.h"
#endif

SceneManager::SceneManager() {}

SceneManager::~SceneManager() {}

void SceneManager::Initialize() {
    // シーンの生成はアプリ層で行う（StatePattern + 層分離）
    // ここではコマンドリストを保存するのみとする
    
}

void SceneManager::Update() {
    // 1. まず現在のシーンの更新を行う
    if (currentScene_) {
        currentScene_->Update(this);
    }

    // 2. 更新が終わった後、シーン遷移を処理する
    ProcessSceneTransition();
}

void SceneManager::ProcessSceneTransition() {
    // 「次のシーン」の予約があるかチェック
    if (nextScene_) {
        // 現在のシーンの終了処理を呼ぶ
        if (currentScene_) {
            currentScene_->OnExit(this);
        }

        // 現在のシーンを削除する前に、Commonに登録されている描画オブジェクトの参照をクリアする
        if (spriteCommon_) spriteCommon_->ClearAll();
        if (modelCommon_) modelCommon_->ClearAll();
        if (particleCommon_) particleCommon_->ClearAll();

        // 現在のシーンを、予約していた新しいシーンに入れ替え
        currentScene_ = std::move(nextScene_);

        // 各種Commonのセット
        if (spriteCommon_)
            currentScene_->SetSpriteCommon(spriteCommon_);
        if (modelCommon_)
            currentScene_->SetModelCommon(modelCommon_);
        if (particleCommon_)
            currentScene_->SetParticleCommon(particleCommon_);
        if (gameCamera_)
            currentScene_->SetGameCamera(gameCamera_);

        // 新しいシーンの初期化
        currentScene_->Initialize();
        
        // 新しいシーンの開始処理を呼ぶ
        currentScene_->OnEnter(this);

#ifdef USE_IMGUI
        if (EditorManager::GetInstance()) {
            EditorManager::GetInstance()->LoadPlacedModelsForScene(currentScene_.get());
        }
#endif
        
        // 初回フレームの描画前に1度Updateを呼び出し、定数バッファやワールド行列をGPUへ完全に同期させる
        currentScene_->Update(this);
    }
}

void SceneManager::Draw(const Matrix4x4 &viewProjectionMatrix) {
    if(currentScene_) {
        currentScene_->Draw(viewProjectionMatrix);
    }
}

void SceneManager::ChangeScene(std::unique_ptr<IScene> nextScene) {
    assert(nextScene); // 渡されたシーンがnullptrでないことを確認

    // currentScene_ が未設定の場合、即時適用して初期化する（アプリ層がシーン生成するため）
    if (!currentScene_) {
        currentScene_ = std::move(nextScene);

        if (spriteCommon_)
            currentScene_->SetSpriteCommon(spriteCommon_);
        if (modelCommon_)
            currentScene_->SetModelCommon(modelCommon_);
        if (particleCommon_)
            currentScene_->SetParticleCommon(particleCommon_);
        if (gameCamera_)
            currentScene_->SetGameCamera(gameCamera_);

        currentScene_->Initialize();
        currentScene_->OnEnter(this);

#ifdef USE_IMGUI
        if (EditorManager::GetInstance()) {
            EditorManager::GetInstance()->LoadPlacedModelsForScene(currentScene_.get());
        }
#endif
        
        // 初回フレームの描画前に1度Updateを呼び出し、定数バッファやワールド行列をGPUへ完全に同期させる
        currentScene_->Update(this);
    } else {
        // 次のフレームで切り替える（安全な遷移）
        nextScene_ = std::move(nextScene);
    }
}

void SceneManager::PushScene(std::unique_ptr<IScene> nextScene) {
    assert(nextScene);
    if (currentScene_) {
        currentScene_->OnExit(this);
        sceneStack_.push_back(std::move(currentScene_));
    }

    if (spriteCommon_) spriteCommon_->ClearAll();
    if (modelCommon_) modelCommon_->ClearAll();
    if (particleCommon_) particleCommon_->ClearAll();

    currentScene_ = std::move(nextScene);
    if (spriteCommon_) currentScene_->SetSpriteCommon(spriteCommon_);
    if (modelCommon_) currentScene_->SetModelCommon(modelCommon_);
    if (particleCommon_) currentScene_->SetParticleCommon(particleCommon_);
    if (gameCamera_) currentScene_->SetGameCamera(gameCamera_);

    currentScene_->Initialize();
    currentScene_->OnEnter(this);

#ifdef USE_IMGUI
    if (EditorManager::GetInstance()) {
        EditorManager::GetInstance()->LoadPlacedModelsForScene(currentScene_.get());
    }
#endif

    currentScene_->Update(this);
}

void SceneManager::PopScene() {
    if (sceneStack_.empty()) return;

    if (currentScene_) {
        currentScene_->OnExit(this);
    }

    if (spriteCommon_) spriteCommon_->ClearAll();
    if (modelCommon_) modelCommon_->ClearAll();
    if (particleCommon_) particleCommon_->ClearAll();

    currentScene_ = std::move(sceneStack_.back());
    sceneStack_.pop_back();

    if (currentScene_) {
        currentScene_->OnEnter(this);
#ifdef USE_IMGUI
        if (EditorManager::GetInstance()) {
            EditorManager::GetInstance()->LoadPlacedModelsForScene(currentScene_.get());
        }
#endif
        currentScene_->Update(this);
    }
}
