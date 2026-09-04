#pragma once
#include "BaseBlock.h"
#include <string>

/// <summary>
/// ドア。同じ連動番号（linkId）のスイッチが押されている間だけ開く
/// - openDirection：開く向き（Up = 上に引っ込む、Down、Left、Right）
/// - openDistance：どれだけ開くか（チップ。0 以下なら全部開く）
/// - latch：一度全部開いたら、スイッチを離しても開いたまま（リトライで戻る）
/// - crushKills：ON = 閉まる時に鎖を挟むとちぎれてミス。OFF = 通路に鎖があると閉まらずに待つ
/// </summary>
class DoorBlock : public BaseBlock {
public:
    DoorBlock(MapChip2D* map, int chipX, int chipY);
    ~DoorBlock() override = default;

    void Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) override;
    void Update() override;

    // ドアは常に動く床扱い（スケールに応じてAABBが変化するため）
    bool IsSolid() const override { return true; }
    bool IsMoving() const override { return true; }

    // 通路に鎖がある（crushKills が OFF の時、閉まらない）
    bool OnChainTouch(const Vector3& pos, float radius, const Vector3& velocity, bool isWeight) override;

    void SetProperties(const nlohmann::json& properties) override;
    void Reset() override;

    // リプレイ対応（開閉の進行度を保存・復元する）
    void CaptureReplayState(std::vector<float>& outCustom) const override;
    void RestoreReplayState(const std::vector<float>& custom) override;

    int GetLinkId() const { return linkId_; }
    const std::string& GetOpenDirection() const { return openDirection_; }
    bool IsLatched() const { return latched_; }
    /// <summary>閉じた状態の枠（エディタの重ね描き用。開いていると本体が潰れて見えないため）</summary>
    AABB2D GetClosedAABB() const;

private:
    void ApplyTransform();

    float startX_ = 0.0f;
    float startY_ = 0.0f;
    float startWidth_ = 1.0f;
    float startHeight_ = 1.0f;

    int linkId_ = 1;
    float openProgress_ = 0.0f; // 0.0f (閉) ～ 1.0f (開)
    float openSpeed_ = 2.0f;    // 開く速度
    float closeSpeed_ = 2.0f;   // 閉まる速度
    std::string openDirection_ = "Up";
    float openDistance_ = 0.0f; // 0 以下 = 全部開く
    bool latch_ = false;
    bool crushKills_ = true;

    bool latched_ = false;      // latch_ で開いたまま固定中
    bool blockedThisFrame_ = false; // 今フレーム通路に鎖があった（OnChainTouch で立ち、Update で消費）
};
