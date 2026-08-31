#include "SceneFactory.h"
#include "Scenes/TitleScene.h"
#include "Scenes/StageSelectScene.h"
#include "Scenes/GameScene.h"
#include "Scenes/BlenderLevelTestScene.h"

std::unique_ptr<IScene> SceneFactory::CreateScene(SceneType type) {
    switch (type) {
    case SceneType::kTitle:
        return std::make_unique<TitleScene>();
    case SceneType::kStageSelect:
        return std::make_unique<StageSelectScene>();
    case SceneType::kGame:
        return std::make_unique<GameScene>();
    case SceneType::kBlenderLevelTest:
        return std::make_unique<BlenderLevelTestScene>();
    default:
        return std::make_unique<TitleScene>();
    }
}

SceneType SceneFactory::GetSceneTypeFromName(const std::string& name) {
    for (int i = 0; i < static_cast<int>(SceneType::kCount); ++i) {
        if (name == kSceneTypeNames[i]) {
            return static_cast<SceneType>(i);
        }
    }
    // デフォルトはタイトルシーン
    return SceneType::kTitle;
}

const char* SceneFactory::GetSceneTypeName(SceneType type) {
    int index = static_cast<int>(type);
    if (index >= 0 && index < static_cast<int>(SceneType::kCount)) {
        return kSceneTypeNames[index];
    }
    return kSceneTypeNames[0];
}
