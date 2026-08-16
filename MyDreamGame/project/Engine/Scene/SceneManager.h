#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include <any>
#include <Windows.h>
#include <format>
#include "IScene.h"

class SpriteCommon;
class ModelCommon;
class ParticleCommon;
class GameCamera;

class SceneManager {
public:
    SceneManager();
    ~SceneManager();

    void Initialize();
    void Update();
    void Draw(const Matrix4x4 &viewProjectionMatrix);

    // シーン遷移のみを処理する（再生状態に関係なく毎フレーム呼ぶ）
    void ProcessSceneTransition();

    IScene *GetCurrentScene() const { return currentScene_.get(); }

    void ChangeScene(std::unique_ptr<IScene> nextScene);

    // SpriteCommonをセットする関数
    void SetSpriteCommon(SpriteCommon *spriteCommon) { spriteCommon_ = spriteCommon; }

    void SetModelCommon(ModelCommon *modelCommon) {
        OutputDebugStringA(std::format("[DEBUG] SceneManager::SetModelCommon - ptr: {}\n", (void*)modelCommon).c_str());
        modelCommon_ = modelCommon;
    }
    ParticleCommon *GetParticleCommon() const { return particleCommon_; }
    void SetParticleCommon(ParticleCommon* particleCommon) {
        particleCommon_ = particleCommon;
    }

    // GameCameraをセットする関数
    void SetGameCamera(GameCamera* gameCamera) { gameCamera_ = gameCamera; }

    // SpriteCommonを取得する関数
    SpriteCommon *GetSpriteCommon() const { return spriteCommon_; }
    ModelCommon *GetModelCommon()const { return modelCommon_; }

    // --- ブラックボード（シーン間データ共有） ---
    template<typename T>
    void SetData(const std::string& key, const T& value) {
        blackboard_[key] = value;
    }

    template<typename T>
    T GetData(const std::string& key) const {
        auto it = blackboard_.find(key);
        if (it != blackboard_.end()) {
            return std::any_cast<T>(it->second);
        }
        return T{};
    }

    bool HasData(const std::string& key) const {
        return blackboard_.find(key) != blackboard_.end();
    }

    void ClearData() { blackboard_.clear(); }

private:
    std::unique_ptr<IScene> currentScene_ = nullptr;
    
    SpriteCommon *spriteCommon_ = nullptr;
    ModelCommon *modelCommon_ = nullptr;
    ParticleCommon* particleCommon_ = nullptr;
    GameCamera* gameCamera_ = nullptr;

    // 次のシーンを予約しておく変数
    std::unique_ptr<IScene> nextScene_ = nullptr;

    // データ共有用の辞書
    std::unordered_map<std::string, std::any> blackboard_;
};
