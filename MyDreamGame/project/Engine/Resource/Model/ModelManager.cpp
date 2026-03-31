#include "ModelManager.h"

void ModelManager::Initialize(ModelCommon *modelCommon) {
    modelCommon_ = modelCommon;
}

void ModelManager::Finalize() {
    modelRegistry.clear();
}

Model *ModelManager::GetModel(const std::string &directoryPath, const std::string &filename) {
    std::string filePath = directoryPath + "/" + filename;

    // すでにロード済みか確認
    if (modelRegistry.find(filePath) == modelRegistry.end()) {
        // ロードされていなければ、Modelを新しく生成
        auto newModel = std::make_unique<Model>();

        // Model自身のInitializeを呼ぶ（これでバッファも一度だけ作られる！）
        newModel->Initialize(modelCommon_, directoryPath, filename);

        modelRegistry[filePath] = std::move(newModel);
    }
    return modelRegistry[filePath].get();
}