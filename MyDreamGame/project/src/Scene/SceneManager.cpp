#include "SceneManager.h"
#include <cassert>

SceneManager::SceneManager() {}

SceneManager::~SceneManager() {}

void SceneManager::Initialize(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList) {
    // シーンの生成はアプリ層で行う（StatePattern + 層分離）
    // ここではコマンドリストを保存するのみとする
    commandList_ = commandList;
}

void SceneManager::Update() {
    // 1. まず現在のシーンの更新を行う
    if (currentScene_) {
        currentScene_->Update(this);
    }

    // 2. 更新が終わった後、「次のシーン」の予約があるかチェック
    if (nextScene_) {
        // 現在のシーンを、予約していた新しいシーンに入れ替え
        currentScene_ = std::move(nextScene_);

        // 各種Commonのセット
        if (spriteCommon_)
            currentScene_->SetSpriteCommon(spriteCommon_);
        if (modelCommon_)
            currentScene_->SetModelCommon(modelCommon_);
        if (particleCommon_)
            currentScene_->SetParticleCommon(particleCommon_);

        // 新しいシーンの初期化
        currentScene_->Initialize(commandList_);
    }
}

void SceneManager::Draw(const Matrix4x4 &viewProjectionMatrix) {
    if(currentScene_) {
        currentScene_->Draw(viewProjectionMatrix);
    }
}

void SceneManager::ChangeScene(IScene *newScene) {
    assert(newScene); // 渡されたシーンがnullptrでないことを確認

    // currentScene_ が未設定の場合、即時適用して初期化する（アプリ層がシーン生成するため）
    if (!currentScene_) {
        currentScene_.reset(newScene);

        if (spriteCommon_)
            currentScene_->SetSpriteCommon(spriteCommon_);
        if (modelCommon_)
            currentScene_->SetModelCommon(modelCommon_);
        if (particleCommon_)
            currentScene_->SetParticleCommon(particleCommon_);

        currentScene_->Initialize(commandList_);
    } else {
        // 次のフレームで切り替える（安全な遷移）
        nextScene_.reset(newScene);
    }
}
