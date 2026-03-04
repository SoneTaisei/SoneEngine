#pragma once
#include "Model.h"
#include <map>
#include <memory>
#include <string>

class ModelManager {
public:
    static ModelManager *GetInstance() {
        static ModelManager instance;
        return &instance;
    }

    // 初期化（Deviceなどが必要な場合）
    void Initialize(ModelCommon *modelCommon);

    void Finalize();

    // モデル（バッファ作成済み）を取得する
    Model *GetModel(const std::string &directoryPath, const std::string &filename);

private:
    ModelManager() = default;
    ModelCommon *modelCommon_ = nullptr;
    // ModelDataではなく、バッファを持つModelをキャッシュする
    std::map<std::string, std::unique_ptr<Model>> modelRegistry;
};