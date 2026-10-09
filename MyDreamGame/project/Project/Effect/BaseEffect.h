#pragma once
#include <vector>

class PrimitiveObject;

/// <summary>
/// エフェクトの基底クラス（ポリモーフィズム用）
/// </summary>
class BaseEffect {
public:
    virtual ~BaseEffect() = default;

    // 毎フレームの更新処理
    virtual void Update(float deltaTime) = 0;

    // 描画処理
    virtual void Draw() = 0;

    // リセット / 消去処理（リトライ時など）
    virtual void Reset() {}

    // エディタ・ヒエラルキー用のプリミティブ取得
    virtual std::vector<PrimitiveObject*> GetPrimitives() { return {}; }
};
