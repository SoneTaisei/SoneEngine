#pragma once
#include "BaseBlock.h"
#include <memory>

class Object3D;

/// <summary>
/// 収集アイテム（小さい青い宝石）。クリアには関係ないやり込み要素。触れると取れる
/// ステージに 1〜3 個置く想定（数はマップに置いた分だけ。上限は無い）
/// 取った宝石は「ステージをクリアした時点で持っていた分」だけ記録され（CollectibleTracker）、次からは薄く表示される
/// 薄い宝石ももう一度取れて、その回の数には入る。ミスで最初からになれば、その回に取った分は消える
/// 見た目は宝石モデル（お宝と同じモデルの小さい青）。当たり判定は箱（描かない）
/// </summary>
class CollectibleBlock : public BaseBlock {
public:
    using BaseBlock::BaseBlock;

    void Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) override;
    void Update() override;
    void Draw() override;

    // プレイヤーが触れた：取る（演出の後に消える）
    void OnCollision(Player2D* player) override;

    // 壁ではないのですり抜けられる
    bool IsSolid() const override { return false; }
    // 取った後もマップに残す（総数の数え上げ、巻き戻しでの復元）
    bool KeepWhenDestroyed() const override { return true; }

    // 復活（エディタの「復活」等）
    void Reset() override;

    /// <summary>取った後の演出中（まだ消えていない）</summary>
    bool IsCollecting() const { return collectTimer_ >= 0.0f; }
    /// <summary>今回の挑戦で取った（演出中も含む）</summary>
    bool IsCollectedNow() const { return isDestroyed_ || collectTimer_ >= 0.0f; }
    /// <summary>以前のクリアで取ったことがある（薄く表示）</summary>
    bool WasCollectedBefore() const { return collectedBefore_; }

private:
    void EnsureModel();

    std::unique_ptr<Object3D> gem_;
    ID3D12Device* device_ = nullptr; // 非所有
    float centerX_ = 0.0f;
    float centerY_ = 0.0f;
    float time_ = 0.0f;           // ゆらゆら・自転用
    float collectTimer_ = -1.0f;  // 取った後の演出の経過秒（<0 = まだ取っていない）
    bool collectedBefore_ = false;
};
