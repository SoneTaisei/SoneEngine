#pragma once
#include <memory>
#include <string>
#include "IScene.h"

// シーンの種類を定義する列挙型
enum class SceneType {
    kTitle = 0,
    kStageSelect,
    kGame,

    kCount // シーンの数を取得するために末尾に置く
};

// シーン名の文字列配列（UI表示・JSON保存用）
static const char* kSceneTypeNames[] = {
    "Title",
    "StageSelect",
    "Game"
};

// シーンのファクトリクラス
class SceneFactory {
public:
    // シーンを生成する
    static std::unique_ptr<IScene> CreateScene(SceneType type);

    // 文字列からシーンタイプを取得する
    static SceneType GetSceneTypeFromName(const std::string& name);

    // シーンタイプから文字列を取得する
    static const char* GetSceneTypeName(SceneType type);
};
