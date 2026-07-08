#include "SceneManager.h"
#include <cassert>
#include "Resource/Sprite/SpriteCommon.h"
#include "Resource/Model/ModelCommon.h"
#include "Effect/ParticleCommon.h"

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
        
        // 初回フレームの描画前に1度Updateを呼び出し、定数バッファやワールド行列をGPUへ完全に同期させる
        currentScene_->Update(this);
    } else {
        // 次のフレームで切り替える（安全な遷移）
        nextScene_ = std::move(nextScene);
    }
}
